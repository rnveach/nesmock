template<typename Movie>
class VirtuaNESMovie: public Movie
{
    // don't add member vars here.

    class Statetype: public Movie::SaveState
    {
        // don't add member vars here either.
    public:
        void Load(const std::vector<unsigned char>& data)
        {
            // all data is ignored for now.
        }
        void Write(std::vector<unsigned char>& data)
        {
        }
    };
public:
    bool Load(const std::vector<unsigned char>& data)
    {
        if(ReadAscii(&data[0], 12) != L"VirtuaNES MV") return false;
        
        { unsigned char TmpBuf[64];
        sprintf((char*)TmpBuf, "%.9s %u.%u%u", &data[0],
            data[0x0F], data[0x0E] >> 4, data[0x0E] & 15);
        EmuName = ReadAscii(TmpBuf,64); }
        
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
        unsigned CtrlBegin = R32(data[0x34]);
        
        FrameCount = R32(data[0x38]);
        
        // TODO: Movie CRC (0x3C)
        
        std::vector<unsigned char> statedata(data.begin()+SaveBegin, data.begin()+SaveEnd);

        (reinterpret_cast<Statetype&>(State)).Load(statedata);
        
        Cdata.SetSize(FrameCount+1);
        
        unsigned pos = CtrlBegin;
        for(unsigned frame=0; frame<FrameCount; ++frame)
        {
            const bool masks[4] = {Ctrl1,Ctrl2,Ctrl3,Ctrl4};
            for(unsigned ctrl=0; ctrl<4; ++ctrl)
            {
                if(!masks[ctrl]) continue;
               
                /* Check for commands */
                for(;;)
                {
                    unsigned char Data = data[pos];
                    if(Data < 0xF0) break;
                    /* It's a command */
                    ++pos;
                    if(Data == 0xF0)
                    {
                        unsigned char byte1 = data[pos++];
                        unsigned char byte2 = data[pos++];
                        if(byte2 == 0)
                        {
                            /* command (NESCOMMAND) byte1, 0 */
                        }
                        else
                        {
                            /* command NESCMD_EXCONTROLLER, byte1 */
                            /* sets the extra controller to given value */
                        }
                    }
                    else if(Data == 0xF3)
                    {
                        /* 4 bytes as params */
                        unsigned dwdata = R32(data[pos]); pos += 4;
                        /* SetSyncExData(dwdata) */
                    }
                }

                Cdata[frame].Ctrl[ctrl] = data[pos++];
            }
        }
        return true;
    }

    void Write(std::vector<unsigned char>& data)
    {
    }
};
