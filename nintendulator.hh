template<typename SaveState>
class NintendulatorSavestate: public SaveState
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
class NintendulatorMovie: public Movie
{
    // don't add member vars here.
public:
    bool Load(const std::vector<unsigned char>& data)
    {
        unsigned long cbname, cblen, cboffset;
        if(R32(data[0x0]) != R32(*"NSS\032")) return false;    // NSS+1A
        if(R32(data[0x4]) != R32(*"0950")) return false;    // 0950 []
                                // [file length]
        if(R32(data[0xC]) != R32(*"NMOV")) return false;    // NMOV

        cboffset = 0x10;
        Save = false;
        bool hasmovie = false;
        while (cboffset < data.size())
        {
            cbname = R32(data[cboffset]);
            cblen = R32(data[cboffset+4]);

            /* TODO: Read save if exists */

            if (cbname == R32(*"NMOV"))
            {
                if (data[cboffset+8] == 5)
                {
                    Ctrl1 = data[cboffset+9] & 0x01;
                    Ctrl2 = data[cboffset+9] & 0x02;
                    Ctrl3 = data[cboffset+9] & 0x04;
                    Ctrl4 = data[cboffset+9] & 0x08;
                }
                else
                {
                    if (data[cboffset+8] > 1) return false; // not a normal controller
                    if (data[cboffset+9] > 1) return false; // not a normal controller
                    Ctrl1 = data[cboffset+8];
                    Ctrl2 = data[cboffset+9];
                    Ctrl3 = Ctrl4 = false;
                }
                if (data[cboffset+10]) return false; // expansion port controller

                unsigned bytes_per_frame = 0;
                if(Ctrl1) ++bytes_per_frame;
                if(Ctrl2) ++bytes_per_frame;
                if(Ctrl3) ++bytes_per_frame;
                if(Ctrl4) ++bytes_per_frame;

                // dangerous assumption, this could be for another mapper
                FDS = (data[cboffset+11] & 0x7F) == (bytes_per_frame+1);
                
                if(FDS)   ++bytes_per_frame;

                PAL = data[cboffset+11] & 0x80;
                
                unsigned info_length = R32(data[cboffset+16]);

                FrameCount = R32(data[cboffset+20+info_length]) / bytes_per_frame;
                RecordCount = R32(data[cboffset+12]);

                EmuName   = L"Nintendulator 0.950";
                MovieName = ReadAscii(&data[cboffset+20], info_length);

                Cdata.SetSize(FrameCount);

                unsigned pos = cboffset+24+info_length;
                unsigned char lastdisk = 0;
                for(unsigned frame=0; frame<FrameCount; ++frame)
                {
                    if(Ctrl1) Cdata[frame].Ctrl[0] = data[pos++];
                    if(Ctrl2) Cdata[frame].Ctrl[1] = data[pos++];
                    if(Ctrl3) Cdata[frame].Ctrl[2] = data[pos++];
                    if(Ctrl4) Cdata[frame].Ctrl[3] = data[pos++];
                    if(FDS)
                    {
                        if (data[pos])
                            lastdisk = (data[pos] == 0xFF) ? 0 : data[pos];
                        Cdata[frame].FDS = lastdisk;
                        pos++;
                    }
                }
                hasmovie = true;
            }
            cboffset += cblen + 8;
        }
        return hasmovie;
    }

    void Write(std::vector<unsigned char>& data)
    {
    }
};
