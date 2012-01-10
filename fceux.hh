class FCEUXMovie: public Movie
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
        unsigned position = 0;
        int fourscore=0, port0=0, port1=0, port2=0;

        PAL  = false;
        Save = false;

        bool version_found = false;
        while(position < data.size() && data[position] != '|')
        {
            unsigned len = 0;
            std::wstring line = ReadLine(&data[position], data.size()-position, len);
            position += len;

            size_t sep = 0;
            while(sep < line.size() && line[sep] != L' ') ++sep;
            std::wstring content(line.substr(sep+1));
            line.erase(line.begin()+sep, line.begin()+line.size());

            if(line == L"version" && content == L"3") version_found = true;
            if(line == L"palFlag") PAL = content == L"1";
            if(line == L"romFilename") ROMName = content;
            if(line == L"comment") MovieName = content;
            if(line == L"emuVersion") std::swscanf(content.c_str(), L"%d", &FCEUversion);
            if(line == L"rerecordCount") std::swscanf(content.c_str(), L"%d", &RecordCount);
            if(line == L"fourscore") std::swscanf(content.c_str(), L"%d", &fourscore);
            if(line == L"port0") std::swscanf(content.c_str(), L"%d", &port0);
            if(line == L"port1") std::swscanf(content.c_str(), L"%d", &port1);
            if(line == L"port2") std::swscanf(content.c_str(), L"%d", &port2);
        }
        if(!version_found) return false;

        if(fourscore)
            Ctrl1 = Ctrl2 = Ctrl3 = Ctrl4 = true;
        else
        {
            Ctrl4 = false;
            Ctrl3 = port2 == 1;
            Ctrl2 = port1 == 1;
            Ctrl1 = port0 == 1;
        }
        Save = false;

        FrameCount = 0;
        Cdata.clear();
        bool status[4] = {Ctrl1,Ctrl2,Ctrl3,Ctrl4};
        while(position < data.size() && data[position] == '|')
        {
            StatusMap ctrl = { {0,0,0,0},0,false };

            unsigned len = 0;
            std::wstring line = ReadLine(&data[position], data.size()-position, len);
            position += len;

            int c = 0;
            std::swscanf(line.c_str(), L"|%d", &c);
            for(len=2; len < line.size() && line[len] != '|'; ) ++len;

            if(c & 1) ctrl.Reset = true;
            if(c & 2) Save = false;
            if(c & 4) ctrl.FDS |= 1;
            if(c & 8) ctrl.FDS |= 4;

            for(unsigned n=0; n<4; ++n)
                if(status[n] && len < line.size() && line[len++] == L'|')
                    for(unsigned b=0x80; b; b>>=1)
                        ctrl.Ctrl[n] |= (line[len++]!=L'.' ? b : 0);
            Cdata.push_back(ctrl);
        }
        FrameCount = Cdata.size();
        return true;
    }

    void Write(std::vector<unsigned char>& data)
    {
        StrPrint(data, "version 3\n");
        StrPrint(data, "emuVersion %d\n", FCEUversion);
        StrPrint(data, "rerecordCount %d\n", RecordCount);
        StrPrint(data, "palFlag %d\n", PAL?1:0);
        StrPrint(data, "FDS %d\n", FDS?1:0);
        StrPrint(data, "romFilename %ls\n", ROMName.c_str());

        bool fourscore = Ctrl4;

        // No romChecksum, guid
        StrPrint(data, "fourscore %d\n", fourscore ? 1 : 0);
        StrPrint(data, "comment %ls\n", MovieName.c_str());
        StrPrint(data, "port0 %d\n", Ctrl1?1:0);
        StrPrint(data, "port1 %d\n", Ctrl2?1:0);
        StrPrint(data, "port2 %d\n", (fourscore ? 0 : (Ctrl3?1:0)));

        bool states[4] = {fourscore||Ctrl1,fourscore||Ctrl2,fourscore||Ctrl3,fourscore||Ctrl4};

        for(unsigned a=0; a<FrameCount; ++a)
        {
            int c = 0;
            if(a == 0 && !Save)
            {
                if(State.rawdata.empty())
                    c |= 2; // start with a power cycle
                else
                    c |= 1; // start with a reset
            }
            if(Cdata[a].FDS & 1) c |= 4; // FDS insert
            if(Cdata[a].FDS & 4) c |= 8; // FDS select
            if(Cdata[a].Reset) c |= 1; // reset

            StrPrint(data, "|%d", c);

            for(unsigned s=0; s<4; ++s)
                if(states[s])
                    StrPrint(data, "|%c%c%c%c%c%c%c%c",
                        (Cdata[a].Ctrl[s]&0x80) ? 'R' : '.',
                        (Cdata[a].Ctrl[s]&0x40) ? 'L' : '.',
                        (Cdata[a].Ctrl[s]&0x20) ? 'D' : '.',
                        (Cdata[a].Ctrl[s]&0x10) ? 'U' : '.',
                        (Cdata[a].Ctrl[s]&0x08) ? 'T' : '.',
                        (Cdata[a].Ctrl[s]&0x04) ? 'S' : '.',
                        (Cdata[a].Ctrl[s]&0x02) ? 'B' : '.',
                        (Cdata[a].Ctrl[s]&0x01) ? 'A' : '.' );
            StrPrint(data, "||\n");
        }
    }
};
