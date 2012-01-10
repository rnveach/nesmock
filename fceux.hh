namespace FCEUX
{
    static const struct Base64Table
    {
        Base64Table()
        {
            size_t a=0;
            for(a=0; a<256; ++a) data[a] = 0xFF; // mark everything as invalid by default
            // create value->ascii mapping
            a=0;
            for(unsigned char c='A'; c<='Z'; ++c) data[a++] = c; // 0..25
            for(unsigned char c='a'; c<='z'; ++c) data[a++] = c; // 26..51
            for(unsigned char c='0'; c<='9'; ++c) data[a++] = c; // 52..61
            data[62] = '+';                             // 62
            data[63] = '/';                             // 63
            // create ascii->value mapping (but due to overlap, write it to highbit region)
            for(a=0; a<64; ++a) data[data[a]^0x80] = a; // 
            data[((unsigned char)'=') ^ 0x80] = 0;
        }
        unsigned char operator[] (size_t pos) const { return data[pos]; }
    private:
        unsigned char data[256];
    } Base64Table;

    std::wstring BytesToString(const unsigned char* begin, const unsigned char* end)
    {
        unsigned len = end-begin;
        if(len == 1)
            { wchar_t Buf[64]; std::swprintf(Buf,sizeof(Buf), L"%d", *begin); return Buf; }
        if(len == 2)
            { wchar_t Buf[64]; std::swprintf(Buf,sizeof(Buf), L"%d", R16(begin)); return Buf; }
        if(len == 4)
            { wchar_t Buf[64]; std::swprintf(Buf,sizeof(Buf), L"%d", R32(begin)); return Buf; }
        std::wstring ret = L"base64:";
        const unsigned char* src = begin;
        for(unsigned n; len > 0; len -= n)
        {
            unsigned char input[3] = {0,0,0};
            for(n=0; n<3 && n<len; ++n)
                input[n] = *src++;
            wchar_t output[4] =
            {
                Base64Table[ input[0] >> 2 ],
                Base64Table[ ((input[0] & 0x03) << 4) | (input[1] >> 4) ],
                n<2 ? '=' : Base64Table[ ((input[1] & 0x0F) << 2) | (input[2] >> 6) ],
                n<3 ? '=' : Base64Table[ input[2] & 0x3F ]
            };
            ret.append(output, output+4);
        }
        return ret;
    }
    void StringToBytes(const std::wstring& str, unsigned char* begin, unsigned char* end)
    {
        unsigned len = end-begin;
        if(str.substr(0,7) == L"base64:")
        {
            // base64
            unsigned char* tgt = begin;
            for(size_t pos = 7; pos < str.size() && len > 0; )
            {
                unsigned char input[4], converted[4];
                for(int i=0; i<4; ++i)
                {
                    if(pos >= str.size() && i > 0) return; // invalid data
                    input[i]     = str[pos++];
                    if(input[i] & 0x80) return;     // illegal character
                    converted[i] = Base64Table[input[i]^0x80];
                    if(converted[i] & 0x80) return; // illegal character
                }
                unsigned char outpacket[3] =
                {
                    (unsigned char)( (converted[0] << 2) | (converted[1] >> 4) ),
                    (unsigned char)( (converted[1] << 4) | (converted[2] >> 2) ),
                    (unsigned char)( (converted[2] << 6) | (converted[3])      )
                };
                unsigned outlen = (input[2] == '=') ? 1 : (input[3] == '=' ? 2 : 3);
                if(outlen > len) outlen = len; 
                std::memcpy(tgt, outpacket, outlen);
                tgt += outlen;
                len -= outlen;
            }
            return;
        }
        unsigned long value=0;
        std::swscanf(str.c_str(), L"%li", &value);
        while(begin < end)
        {
            *begin++ = value & 0xFF;
            value >>= 8;
        }
    }
}

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
            if(line == L"romChecksum") FCEUX::StringToBytes(content, MD5sum+0, MD5sum+16);
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
        StrPrint(data, "romChecksum %ls\n", FCEUX::BytesToString(MD5sum, MD5sum+16).c_str());

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
