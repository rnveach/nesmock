#include <string>
#include <vector>
#include <stdint.h>
#include <cstring>
#include <cwchar>
#include <cctype>
#include <map>

#include <iostream>
#include <locale>

//#define _GNU_SOURCE
#include <unistd.h>
#include <getopt.h>

#define R32(x) (*const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&(x))))

/* Famtasia buttons. */
#define R 0x01
#define L 0x02
#define U 0x04
#define D 0x08
#define B 0x10
#define A 0x20
#define SE 0x40
#define ST 0x80

static const std::wstring ReadAscii(const unsigned char* s, unsigned len)
{
    std::wstring result;
    while(len--) result += *s++;
    return result;
}
static const std::wstring ReadU16str(const unsigned char* s, unsigned len)
{
    std::wstring result;
    while(len > 0) { result += (*s) | (s[1] << 8); s += 2; }
    return result;
}
static const std::wstring ReadUTF8(const unsigned char* s, unsigned len)
{
    std::wstring result;
    const unsigned char*const end = s+len;
    /* Compared to the beast found in iconvircproxy,
     * this utf8 decoder is quite nice. But it isn't
     * as robust.
     */
    while(s<end)
    {
        unsigned val = *s++;
        if(!val) break;
        switch(val >> 4)
        {
            case 0x0F: val &= 0x07; goto b3;
            case 0x0E: val &= 0x0F; goto b2;
            case 0x0D:
            case 0x0C: val &= 0x1F; goto b1;
            default:   val &= 0x7F; break;
            b3: val <<= 6; val |= (*s++) & 63;
            b2: val <<= 6; val |= (*s++) & 63;
            b1: val <<= 6; val |= (*s++) & 63;
        }
        result += (wchar_t)val;
    }
    return result;
}
static void PutUTF8(std::vector<unsigned char>& buf, const std::wstring& s)
{
    for(unsigned a=0; a<s.size(); ++a)
    {
        unsigned n = s[a];
        if(n < 0x80)                 // <=7 bits
            buf.push_back(n);
        else
        {
            if(n < 0x800)            // <=11 bits
                buf.push_back((0xC0 + (n>>6)));
            else
            {
                if(n < 0x10000)      // <=16 bits
                    buf.push_back((0xE0 + (n>>12)));
                else                 // <=21 bits
                {
                    buf.push_back((0xF0 + (n>>18)));
                    buf.push_back((0x80 + ((n>>12)&63)));
                }
                buf.push_back((0x80 + ((n>>6)&63)));
            }
            buf.push_back((0x80 + (n&63)));
        }
    }
}

static const std::string WsToMb(const std::wstring& s)
{
    char Buf[4096];
    size_t n = std::wcstombs(Buf, s.c_str(), sizeof(Buf));
    return std::string(Buf, Buf+n);
}

static void Write8(std::vector<unsigned char>& target, uint8_t value)
{
    target.push_back(value);
}
static void Write16(std::vector<unsigned char>& target, uint16_t value)
{
    target.push_back(value & 0xFF);
    target.push_back(value >> 8);
}
static void Write32(std::vector<unsigned char>& target, uint32_t value)
{
    Write16(target, value & 0xFFFF);
    Write16(target, value >> 16);
}
static void WriteAsciiFZ(std::vector<unsigned char>& target, const std::wstring& s, unsigned length)
{
    for(unsigned a=0; a<length; ++a)
        target.push_back(a < s.length() ? s[a] : 0);
}

class SaveState
{
    /* CPU data */
    uint16_t regPC;
    uint8_t regA, regP, regX, regY, regS;
    char RAM[0x800];
    /* PPU data */
    char ntaram[0x800];
    char palram[0x20];
    char spram[0x100];
    char ppu[0x04];
    /* Joystick data */
    uint16_t joy_readbit;
    uint32_t joy;
public:
    // load famtasia save
    void LoadFMS(const std::vector<unsigned char>& data)
    {
        // all data is ignored for now.
    }
    // load fceu save
    void LoadFCS(const std::vector<unsigned char>& data)
    {
        // all data is ignored for now.
    }
    // load virtuanes save
    void LoadVMS(const std::vector<unsigned char>& data)
    {
        // all data is ignored for now.
    }
public:
    // create famtasia save
    void WriteFMS(std::vector<unsigned char>& data)
    {
    }
    // create fceu save
    void WriteFCS(std::vector<unsigned char>& data)
    {
        // Create a dummy FCEU save that contains nothing
        // but a header. FCEU will load it gracefully.
        // Nothing more is needed for a power-on savestate.
        std::vector<unsigned char> buf(16);
        
        buf[0] = 'F'; buf[1] = 'C'; buf[2] = 'S';
        
        // copy the state
        data.insert(data.end(), buf.begin(), buf.end());
    }
    // create virtuanes save
    void WriteVMS(std::vector<unsigned char>& data)
    {
    }
};

class Movie
{
    bool PAL;
    bool Save;
    unsigned FrameCount;
    unsigned RecordCount;
    
    bool Ctrl1;
    bool Ctrl2;
    bool Ctrl3;
    bool Ctrl4;
    bool FDS;
    
    std::wstring EmuName;
    std::wstring MovieName;
    std::wstring ROMName;
    
    typedef std::map<unsigned, unsigned char> CtrlDataMap;
    
    CtrlDataMap Cdata[4], FDSdata;
    
    SaveState State;

public:
    Movie()
    {
    }
    Movie(const std::vector<unsigned char>& data)
    {
        Read(data);
    }
    
    void DumpStatus()
    {
        /* Locale support is a MESS, and mingw32 doesn't support wcout at all. */
        std::setlocale(LC_ALL, "");
        std::string lname = std::setlocale(LC_CTYPE, NULL);
        if(lname == "C" || lname == "POSIX") lname = "en_US.UTF-8";
        std::locale loc(lname.c_str());
        std::locale::global(loc);
#if 0
        std::cerr << std::flush;
        std::cout << std::flush;
        std::wcout <<
            L"Movie information:\n"
            L"- PAL:       " << PAL << L"\n"
            L"- Savestate: " << Save << L"\n"
            L"- Frames:    " << FrameCount << L"\n"
            L"- Rerecords: " << RecordCount << L"\n"
            L"- Controllers: ";
        if(Ctrl1) std::wcout << L" P1";
        if(Ctrl2) std::wcout << L" P2";
        if(Ctrl3) std::wcout << L" P3";
        if(Ctrl4) std::wcout << L" P4";
        if(FDS)   std::wcout << L" FDS";
        std::wcout << L"\n"
            L"- Create emu: " << EmuName << L"\n"
            L"- ROM name:   " << ROMName  << L"\n"
            L"- Movie name: " << MovieName << L"\n";
        std::wcout << std::flush;
#else
        std::cout <<
            "Movie information:\n"
            "- PAL:       " << PAL << "\n"
            "- Savestate: " << Save << "\n"
            "- Frames:    " << FrameCount << "\n"
            "- Rerecords: " << RecordCount << "\n"
            "- Controllers: ";
        if(Ctrl1) std::cout << " P1";
        if(Ctrl2) std::cout << " P2";
        if(Ctrl3) std::cout << " P3";
        if(Ctrl4) std::cout << " P4";
        if(FDS)   std::cout << " FDS";
        std::cout << "\n"
            "- Create emu: " << WsToMb(EmuName) << "\n"
            "- ROM name:   " << WsToMb(ROMName)  << "\n"
            "- Movie name: " << WsToMb(MovieName) << "\n";
        std::cout << std::flush;
#endif
    }

    bool Read(const std::vector<unsigned char>& data)
    {
        if(ReadFMV(data)) return true;
        if(ReadFCM(data)) return true;
        if(ReadVMV(data)) return true;
        if(ReadNSM(data)) return true;
        return false;
    }    
    
    bool ReadFMV(const std::vector<unsigned char>& data)
    {
        if(R32(data[0]) != 0x1A564D46) return false;
        
        PAL   = false;
        Save  = data[4] & 0x80;
        Ctrl1 = data[5] & 0x80;
        Ctrl2 = data[5] & 0x40;
        Ctrl3 = Ctrl4 = false;
        FDS   = data[5] & 0x20;
        
        FrameCount = data.size() - 0x90;
        RecordCount = R32(data[0x0A]) + 1;
        
        EmuName   = ReadAscii(&data[0x10], std::strlen((const char*)&data[0x10]));
        MovieName = ReadAscii(&data[0x50], std::strlen((const char*)&data[0x50]));

        /* TODO: Read save if exists */
        
        unsigned pos = 0x90;
        for(unsigned frame=0; frame<FrameCount; ++frame)
        {
            if(Ctrl1) Cdata[0][frame] = data[pos++];
            if(Ctrl2) Cdata[1][frame] = data[pos++];
            if(FDS)   FDSdata[frame]  = data[pos++];
        }
        return true;
    }

    bool ReadFCM(const std::vector<unsigned char>& data)
    {
        if(R32(data[0]) != 0x1A4D4346) return false;
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
        
        /* TODO: Read md5sum */
        
        unsigned rnlength = std::strlen((const char*)&data[0x34]);
        ROMName   = ReadUTF8(&data[0x34], rnlength);
        unsigned mnlength = std::strlen((const char*)&data[0x34+rnlength+1]);
        MovieName = ReadUTF8(&data[0x34+rnlength+1], mnlength);
        
        std::vector<unsigned char> statedata(data.begin()+SaveOffs, data.begin()+CtrlOffs);
        State.LoadFCS(statedata);
        
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
                for(unsigned ctrl=0; ctrl<4; ++ctrl) Cdata[ctrl][frame] = joop[ctrl];
                if(!delta)break;
                ++frame;
                --delta;
            }
            
            if(CtrlLength > NDelta) CtrlLength -= NDelta; else CtrlLength = 0;

            if(Type == false) // Controller data
            {
                unsigned ctrlno = (Data >> 3);
                /* convert FCM buttons to FMV buttons */
                static const unsigned char masks[8] = {A,B,SE,ST,U,D,L,R};
                joop[ctrlno] ^= masks[Data & 7];
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
                    case 2: break; // power cycle
                    case 7: break; // VS coin
                    case 8: break; // VS dip0
                    case 24: FDS=true; FDSdata[frame] |= 1; break; /* FDS insert, FIXME */
                    case 25: FDS=true; FDSdata[frame] |= 2; break; /* FDS eject, FIXME */
                    case 26: FDS=true; FDSdata[frame] |= 4; break; /* FDS swap, FIXME */
                }
                
        }

        return true;
    }

    bool ReadVMV(const std::vector<unsigned char>& data)
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
        State.LoadVMS(statedata);
        
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
                unsigned char Ctrl=0;
                if(Data&0x01) Ctrl|=A;
                if(Data&0x02) Ctrl|=B;
                if(Data&0x04) Ctrl|=SE;
                if(Data&0x08) Ctrl|=ST;
                if(Data&0x10) Ctrl|=U;
                if(Data&0x20) Ctrl|=D;
                if(Data&0x40) Ctrl|=L;
                if(Data&0x80) Ctrl|=R;
                
                /* This may look strange, but it's what VirtuaNES does. */
                while(!Masks[ctrl] && ctrl < 4) ++ctrl;
                switch(ctrl)
                {
                    case 0: Cdata[0][frame] = Ctrl; Ctrl1=true; break;
                    case 1: Cdata[1][frame] = Ctrl; Ctrl2=true; break;
                    case 2: Cdata[2][frame] = Ctrl; Ctrl3=true; break;
                    case 3: Cdata[3][frame] = Ctrl; Ctrl4=true; break;
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

    bool ReadNSM(const std::vector<unsigned char>& data)
    {
        return false;
    }
    
    void WriteFMV(std::vector<unsigned char>& data, int offset=0)
    {
        Write32(data, 0x1A564D46);
        
        Write8(data, 0);
        
        unsigned char tmp = 0x00;
        if(Ctrl1) tmp|= 0x80;
        if(Ctrl2) tmp|= 0x40;
        if(FDS)   tmp|= 0x20;
        Write8(data, tmp);
        
        Write32(data, 0);
        Write32(data, RecordCount-1);
        Write16(data, 0);
        WriteAsciiFZ(data, EmuName, 64);
        WriteAsciiFZ(data, MovieName, 64);
        
        for(int a=offset; a < (int)FrameCount; ++a)
        {
            if(Ctrl1) Write8(data, a<0 ? 0 : Cdata[0][a]);
            if(Ctrl2) Write8(data, a<0 ? 0 : Cdata[1][a]);
            if(FDS)   Write8(data, a<0 ? 0 : FDSdata[a]);
        }
    }
    
    void WriteFCM(std::vector<unsigned char>& data, int offset=0)
    {
        std::vector<unsigned char> CompressedData;
        
        #define FCMPutDeltaCommand(cmd, n, c) \
        { \
            Write8(CompressedData, ((cmd) & 0x9F) | (c << 5)); \
            if(c>=1) Write8(CompressedData, ((n) >> 0) & 0xFF); \
            if(c>=2) Write8(CompressedData, ((n) >> 8) & 0xFF); \
            if(c>=3) Write8(CompressedData, ((n) >>16) & 0xFF); \
            Buffer -= n; \
        }
        
        #define FCMPutCommand(byte) \
        { \
            unsigned char b = (byte); \
            if(Buffer <= 0) { FCMPutDeltaCommand(b, 0, 0);b=0x80; } \
            while(Buffer > 0) \
            { \
                if(Buffer > 0xFFFFFF) { FCMPutDeltaCommand(0x80, 0xFFFFFF,3); } \
                else if(Buffer > 0xFFFF) { FCMPutDeltaCommand(b,Buffer,3);b=0x80; } \
                else if(Buffer > 0xFF)   { FCMPutDeltaCommand(b,Buffer,2);b=0x80; } \
                else /*(Buffer > 0)*/    { FCMPutDeltaCommand(b,Buffer,1);b=0x80; } \
            } \
        }
        
        int Buffer = 0;
        
        if(!Save) FCMPutCommand(0x82); // start with a power cycle
        
        Buffer += offset;
        
        unsigned char joop[4] = {0,0,0,0};
        for(unsigned a=0; a<FrameCount; ++a)
        {
            for(unsigned ctrl=0; ctrl<4; ++ctrl)
            {
                if(ctrl == 0 && !Ctrl1) continue;
                if(ctrl == 1 && !Ctrl2) continue;
                if(ctrl == 2 && !Ctrl3) continue;
                if(ctrl == 3 && !Ctrl4) continue;
                unsigned char c = Cdata[ctrl][a];
                unsigned char diffs = c ^ joop[ctrl];
                if(diffs & A) FCMPutCommand((ctrl<<3) | 0);
                if(diffs & B) FCMPutCommand((ctrl<<3) | 1);
                if(diffs &SE) FCMPutCommand((ctrl<<3) | 2);
                if(diffs &ST) FCMPutCommand((ctrl<<3) | 3);
                if(diffs & U) FCMPutCommand((ctrl<<3) | 4);
                if(diffs & D) FCMPutCommand((ctrl<<3) | 5);
                if(diffs & L) FCMPutCommand((ctrl<<3) | 6);
                if(diffs & R) FCMPutCommand((ctrl<<3) | 7);
                joop[ctrl] = c;
            }
            ++Buffer;
        }
        FCMPutCommand(0x80); // last command = idle
        
        std::vector<unsigned char> SaveState;
        State.WriteFCS(SaveState);

        std::vector<unsigned char> varlen_hdr;
        PutUTF8(varlen_hdr, ROMName);
        Write8(varlen_hdr, 0);
        
        PutUTF8(varlen_hdr, MovieName);
        Write8(varlen_hdr, 0);
        
        while(SaveState.size() & 3)  Write8(SaveState, 0);
        while(varlen_hdr.size() & 3) Write8(varlen_hdr, 0);
        
        Write32(data, 0x1A4D4346);
        Write32(data, 2);
        
        unsigned char tmp = 0x00;
        if(Save) tmp |= 0x02;
        if(PAL)  tmp |= 0x04;
        Write8(data, tmp);
        
        Write8(data, 0x00);
        Write8(data, 0x00);
        Write8(data, 0x00);
        
        Write32(data, FrameCount);
        Write32(data, RecordCount);
        
        Write32(data, CompressedData.size());
        
        Write32(data, 0x34 + varlen_hdr.size());
        Write32(data, 0x34 + varlen_hdr.size() + SaveState.size());
        
        // md5sum - ignore.
        Write32(data,0);Write32(data,0);Write32(data,0);Write32(data,0);
        
        // emulator version - ignore.
        Write32(data, 0);
        
        data.insert(data.end(), varlen_hdr.begin(), varlen_hdr.end());
        data.insert(data.end(), SaveState.begin(), SaveState.end());
        
        data.insert(data.end(), CompressedData.begin(), CompressedData.end());
    }
    
    void WriteVMV(std::vector<unsigned char>& data)
    {
    }
    
    void WriteNSM(std::vector<unsigned char>& data)
    {
    }
};


static const std::vector<unsigned char> LoadFile(const std::string& fn)
{
    std::vector<unsigned char> buf;
    
    FILE *fp = std::fopen(fn.c_str(), "rb");
    if(!fp)
    {
        perror(fn.c_str());
        return buf;
    }
    std::fseek(fp, 0, SEEK_END);
    buf.resize(std::ftell(fp));
    
    std::rewind(fp);
    std::fread(&buf[0], 1, buf.size(), fp);
    std::fclose(fp);
    
    return buf;
}

static void WriteFile(const std::vector<unsigned char>& buf, const std::string& fn)
{
    FILE *fp = std::fopen(fn.c_str(), "wb");
    if(!fp)
    {
        perror(fn.c_str());
        return;
    }
    std::fwrite(&buf[0], 1, buf.size(), fp);
    std::fclose(fp);
}

static void ConvertToFMV(const std::string& sourcefilename,
                         const std::string& resultfilename,
                         int offset)
{
    Movie movie(LoadFile(sourcefilename));
    
    movie.DumpStatus();
    
    std::vector<unsigned char> file2;
    
    movie.WriteFMV(file2, offset);
    
    WriteFile(file2, resultfilename);
}

static void ConvertToFCM(const std::string& sourcefilename,
                         const std::string& resultfilename,
                         int offset)
{
    Movie movie(LoadFile(sourcefilename));
    
    movie.DumpStatus();
    
    std::vector<unsigned char> file2;
    
    movie.WriteFCM(file2, offset);
    
    WriteFile(file2, resultfilename);
}

static const std::string GetExt(const std::string& fn)
{
    std::string result;
    unsigned b=fn.size();
    while(b > 0 && fn[--b] != '.')
        result.insert(0, 1, std::tolower(fn[b]));
    return result;
}

int main(int argc, char** argv)
{
    int offset = 0;
    bool offset_specified = false;
    for(;;)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            {"help",     0, 0,'h'},
            {"version",  0, 0,'V'},
            {"offset",   1, 0,'o'},
            {0,0,0,0}
        };
        int c = getopt_long(argc, argv, "hVo:", long_options, &option_index);
        if(c==-1) break;
        switch(c)
        {
            case 'V':
            {
                std::cout << VERSION"\n";
                return 0;
            }
            case 'h':
            {
                std::cout << 
                    "nesmock v"VERSION" - Copyright (C) 1992,2004 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "\n"
                    "Usage: nesmock [<options>] <inputfile> <outputfile>\n"
                    "\n"
                    "Transforms NES movie files to different formats.\n"
                    " --help, -h     This help\n"
                    " --offset, -o   Set frame offset\n"
                    " --version, -V  Displays version information\n"
                    "\n"
                    "Supported formats:\n"
                    "  FMV  (Famtasia 5.1) - Read & Write\n"
                    "  FCM  (FCEU 0.98.12) - Read & Write\n"
                    "  VMV  (VirtuaNES)    - Read\n"
                 // "  NSM  (Nesticle)     - none\n"
                    "\n"
                    "Example:\n"
                    "  nesmock -o2 smb1a.fcm smb1a.fmv\n"
                    "\n";
                return 0;
            }
            case 'o':
            {   
                if(offset_specified)
                    std::cerr << "nesmock: warning: -o flag given twice\n";
                offset_specified = true;
                char* arg = optarg;
                offset = strtol(arg, &arg, 10);
                if(*arg)
                    std::cerr << "nesmock: warning: `" << optarg << "' is not a valid base-10 integer\n";
                break;
            }
        }
    }
    if(argc != optind+2)
    {
        std::cerr << "nesmock: Invalid parameters. Try `nesmock --help'\n";
        return 1;
    }
    
    std::cout << "Using offset " << offset << ".\n";

    const std::string fn1 = argv[optind+0];
    const std::string fn2 = argv[optind+1];
    
    std::string ext = GetExt(fn2);
    if(ext == "fmv")
    {
        ConvertToFMV(fn1, fn2, offset);
    }
    else if(ext == "fcm")
    {
        ConvertToFCM(fn1, fn2, offset);
    }
    else
    {
        std::cerr << "nesmock: Unsupported target format `" << ext << "'\n"
                     " - Can not convert `" << fn1 << "' to `" << fn2 << "'\n"
                     "See `nesmock --help' for details.\n";
    }
    std::cout << "Done\n";
    
    return 0;
}
