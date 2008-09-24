class FCEUMovie: public Movie
{
    // don't add member vars here.
    
    class Statetype: public Movie::SaveState
    {
        // don't add member vars here either.
    public:
        void Load(const std::vector<unsigned char>& data)
        {
            // all data is ignored for now.
            rawdata = data;
        }
        void Write(std::vector<unsigned char>& data)
        {
            if(!rawdata.empty())
            {
                data = rawdata;
                return;
            }
            // Create a dummy FCEU save that contains nothing
            // but a header. FCEU will load it gracefully.
            // Nothing more is needed for a power-on savestate.
            std::vector<unsigned char> buf(16);
            
            buf[0] = 'F'; buf[1] = 'C'; buf[2] = 'S';
            
            // copy the state
            data.insert(data.end(), buf.begin(), buf.end());
        }
    };
    
public:
    bool Load(const std::vector<unsigned char>& data)
    {
        if(R32(data[0]) != FOURCC("FCM\032")) return false;
        if(R32(data[4]) != 2) return false;
        
        PAL  = data[8] & 0x04;
        Save = true; /* Set to false if Reset command encountered. */
        Ctrl1 = Ctrl2 = Ctrl3 = Ctrl4 = FDS = false;
        /* ^ fill in when encountered in input. */
        FrameCount  = R32(data[0x0C]);
        RecordCount = R32(data[0x10]);
        unsigned SaveOffs = R32(data[0x18]);
        unsigned CtrlOffs = R32(data[0x1C]);
        unsigned CtrlLength = R32(data[0x14]);
        for(unsigned a=0; a<16; ++a)
            MD5sum[a] = data[0x20+a];
        FCEUversion = R32(data[0x30]);
        
        Cdata.SetSize(FrameCount+1);
        
        unsigned rnlength = std::strlen((const char*)&data[0x34]);
        ROMName   = ReadUTF8(&data[0x34], rnlength);
        unsigned mnlength = std::strlen((const char*)&data[0x34+rnlength+1]);
        MovieName = ReadUTF8(&data[0x34+rnlength+1], mnlength);
        
        std::vector<unsigned char> statedata(data.begin()+SaveOffs, data.begin()+CtrlOffs);
        
        (reinterpret_cast<Statetype&>(State)).Load(statedata);
        
        unsigned char joop[4] = {0,0,0,0};
        
        unsigned frame = 0, pos = CtrlOffs;
        while(CtrlLength > 0)
        {
            bool     Type   = data[pos] >> 7;
            unsigned NDelta = (data[pos] >> 5) & 3;
            unsigned Data   = data[pos] & 0x1F;
            ++pos; --CtrlLength;
            
            unsigned delta = 0;
            switch(NDelta)
            {
               case 0: break;
               case 1: delta |= data[pos++]; break;
               case 2: delta |= data[pos++];
                       delta |= data[pos++] << 8; break;
               case 3: delta |= data[pos++];
                       delta |= data[pos++] << 8;
                       delta |= data[pos++] << 16; break;
            }

            for(;;)
            {
                /* Save the controlled data */
                for(unsigned ctrl=0; ctrl<4; ++ctrl)
                    Cdata[frame].Ctrl[ctrl] = joop[ctrl];
                
                if(!delta)break;
                ++frame;
                --delta;
            }
            
            if(CtrlLength > NDelta) CtrlLength -= NDelta; else CtrlLength = 0;

            if(Type == false) // Controller data
            {
                unsigned ctrlno = (Data >> 3);
                joop[ctrlno] ^= (1 << (Data & 7));
                if(ctrlno == 0) { Ctrl1 = true; }
                if(ctrlno == 1) { Ctrl2 = true; }
                if(ctrlno == 2) { Ctrl3 = true; }
                if(ctrlno == 3) { Ctrl4 = true; }
            }
            else // Control data
                switch(Data)
                {
                    case 0: break; // nothing
                    case 1: Save=false; break; // reset
                    case 2: Save=false; break; // power cycle
                    case 7: break; // VS coin
                    case 8: break; // VS dip0
                    case 24: FDS=true; Cdata[frame].FDS |= 1; break; /* FDS insert, FIXME */
                    case 25: FDS=true; Cdata[frame].FDS |= 2; break; /* FDS eject, FIXME */
                    case 26: FDS=true; Cdata[frame].FDS |= 4; break; /* FDS swap, FIXME */
                }
        }

        return true;
    }

    void Write(std::vector<unsigned char>& data)
    {
        std::vector<unsigned char> CompressedData;
        
        #define FCMPutDeltaCommand(cmd, n, c) \
        do { \
            Write8(CompressedData, ((cmd) & 0x9F) | (c << 5)); \
            if(c>=1) Write8(CompressedData, ((n) >> 0) & 0xFF); \
            if(c>=2) Write8(CompressedData, ((n) >> 8) & 0xFF); \
            if(c>=3) Write8(CompressedData, ((n) >>16) & 0xFF); \
            Buffer -= n; \
        } while(0)
        
        #define FCMPutCommand(byte) \
        do { \
            unsigned char b = (byte); \
            if(Buffer <= 0) { FCMPutDeltaCommand(b, 0, 0);b=0x80; } \
            while(Buffer > 0) \
            { \
                if(Buffer > 0xFFFFFF) { FCMPutDeltaCommand(0x80, 0xFFFFFF,3); } \
                else if(Buffer > 0xFFFF) { FCMPutDeltaCommand(b,Buffer,3);b=0x80; } \
                else if(Buffer > 0xFF)   { FCMPutDeltaCommand(b,Buffer,2);b=0x80; } \
                else /*(Buffer > 0)*/    { FCMPutDeltaCommand(b,Buffer,1);b=0x80; } \
            } \
        } while(0)
        
        int Buffer = 0;
        
        if(!Save)
        {
            if(State.rawdata.empty())
                FCMPutCommand(0x82); // start with a power cycle
            else
                FCMPutCommand(0x81); // start with a reset
        }
        
        unsigned char joop[4] = {0,0,0,0};
        for(unsigned a=0; a<FrameCount; ++a)
        {
            for(unsigned ctrl=0; ctrl<4; ++ctrl)
            {
                if(ctrl == 0 && !Ctrl1) continue;
                if(ctrl == 1 && !Ctrl2) continue;
                if(ctrl == 2 && !Ctrl3) continue;
                if(ctrl == 3 && !Ctrl4) continue;
                unsigned char c = Cdata[a].Ctrl[ctrl];
                unsigned char diffs = c ^ joop[ctrl];
                for(unsigned a=0; a<8; ++a)
                    if(diffs & (1 << a)) FCMPutCommand((ctrl<<3) | a);
                joop[ctrl] = c;
            }
            ++Buffer;
        }
        FCMPutCommand(0x80); // last command = idle
        
        std::vector<unsigned char> statedata;
        (reinterpret_cast<Statetype&>(State)).Write(statedata);

        std::vector<unsigned char> varlen_hdr;
        PutUTF8(varlen_hdr, ROMName);
        Write8(varlen_hdr, 0);
        
        PutUTF8(varlen_hdr, MovieName);
        Write8(varlen_hdr, 0);
        
        while(statedata.size() & 3)  Write8(statedata, 0);
        while(varlen_hdr.size() & 3) Write8(varlen_hdr, 0);
        
        Write32(data, FOURCC("FCM\032"));
        Write32(data, 2);
        
        unsigned char tmp = 0x00;
        if(!Save)tmp |= 0x02;
        if(PAL)  tmp |= 0x04;
        Write8(data, tmp);
        
        Write8(data, 0x00);
        Write8(data, 0x00);
        Write8(data, 0x00);
        
        Write32(data, FrameCount);
        Write32(data, RecordCount);
        
        Write32(data, CompressedData.size());
        
        Write32(data, 0x34 + varlen_hdr.size());
        Write32(data, 0x34 + varlen_hdr.size() + statedata.size());
        
        // md5sum - ignore.
        for(unsigned a=0; a<16; ++a)
            Write8(data, MD5sum[a]);
        
        // emulator version - ignore.
        Write32(data, FCEUversion);
        
        data.insert(data.end(), varlen_hdr.begin(), varlen_hdr.end());
        data.insert(data.end(), statedata.begin(), statedata.end());
        
        data.insert(data.end(), CompressedData.begin(), CompressedData.end());
    }
};
