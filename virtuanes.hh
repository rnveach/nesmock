template<typename SaveState>
class VirtuaNESSavestate: public SaveState
{
    // don't add member vars here.
public:
    void Load(const std::vector<unsigned char>& data)
    {
        // all data is ignored for now.
    }
    void Write(std::vector<unsigned char>& data)
    {
    }
};

template<typename Movie>
class VirtuaNESMovie: public Movie
{
    // don't add member vars here.

    typedef VirtuaNESSavestate<typename Movie::SaveState> Statetype;
    
public:
    bool Load(const std::vector<unsigned char>& data)
    {
        if(ReadAscii(&data[0], 12) != L"VirtuaNES MV") return false;
        
        Save  = true;
        Ctrl1 = data[0x10] & 0x01;
        Ctrl2 = data[0x10] & 0x02;
        Ctrl3 = data[0x10] & 0x04;
        Ctrl4 = data[0x10] & 0x08;
        
        RecordCount = R32(data[0x1C]);
        // TODO: ROM CRC
        // TODO: render method
        // TODO: irq type
        // TODO: frame ireq
        
        PAL = data[0x23] & 0x1;
        
        unsigned SaveBegin = R32(data[0x2C]);
        unsigned SaveEnd   = R32(data[0x30]);
        unsigned CtrlBegin = R32(data[0x2C]);
        unsigned CtrlEnd   = R32(data[0x30]);
        
        // TODO: Movie CRC
        
        std::vector<unsigned char> statedata(data.begin()+SaveBegin, data.begin()+SaveEnd);

        (reinterpret_cast<Statetype&>(State)).Load(statedata);
        
        bool Masks[4] = {Ctrl1,Ctrl2,Ctrl3,Ctrl4};
        
        unsigned pos   = CtrlBegin;
        unsigned frame = 0;
        unsigned ctrl  = 0;
        while(pos < CtrlEnd)
        {
            unsigned char Data = data[pos++];
            if(Data >= 0xF0) // command?
            {
                if(Data == 0xF0)
                {
                    if(data[pos+1] == 0)
                    { /* command (NESCOMMAND) data[pos], 0 */ }
                    else
                    { /* command NESCMD_EXCONTROLLER, data[pos] */
                      /* sets the extra controller to given value */
                    }

                    pos += 2;
                }
                else if(Data == 0xF3)
                {
                    unsigned dwdata = R32(data[pos]); pos += 4;
                    /* SetSyncExData(dwdata) */
                }
            }
            else
            {
                unsigned char Ctrl = Data;
                
                Cdata.Ensure(frame);
                
                /* This may look strange, but it's what VirtuaNES does. */
                while(!Masks[ctrl] && ctrl < 4) ++ctrl;
                switch(ctrl)
                {
                    case 0: Cdata[frame].Ctrl[0] = Ctrl; Ctrl1=true; break;
                    case 1: Cdata[frame].Ctrl[1] = Ctrl; Ctrl2=true; break;
                    case 2: Cdata[frame].Ctrl[2] = Ctrl; Ctrl3=true; break;
                    case 3: Cdata[frame].Ctrl[3] = Ctrl; Ctrl4=true; break;
                }
                while(!Masks[ctrl] && ctrl < 4) ++ctrl;
                
                if(ctrl >= 4)
                {
                    ctrl = 0;
                    ++frame;
                }
            }
        }
        return true;
    }

    void Write(std::vector<unsigned char>& data)
    {
    }
};
