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

#include "utility.hh"
#include "md5.hh"

#include "famtasia.hh"
#include "fceu.hh"
#include "virtuanes.hh"
#include "nintendulator.hh"
#include "nesticle.hh"

class MovieConfigurator;
class Movie
{
protected:
    friend class MovieConfigurator;

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
    
    unsigned char MD5sum[16];
    unsigned FCEUversion;
    
    struct StatusMap
    {
        unsigned char Ctrl[4];
        unsigned char FDS;
    };
    struct ControlData: public std::vector<StatusMap>
    {
        /* Set the length precisely */
        void SetSize(unsigned n)
        {
            this->resize(n);
        }
        
        /* Ensure we can write to the given index */
        void Ensure(unsigned n)
        {
            if(this->size() <= n)
            {
                const unsigned increment = 256;
                unsigned newsize = (n+increment) & ~(increment-1);
                SetSize(newsize);
            }
        }
        
        void ApplyDelay(unsigned frameno, long offset)
        {
            if(offset > 0)
            {
                std::cout << "Inserting " << offset << " frames at " << frameno << std::endl;
                this->insert(this->begin() + frameno,
                             offset,
                             (*this)[frameno]);
            }
            else if(offset < 0)
            {
                std::cout << "Deleting " << -offset << " frames at " << frameno << std::endl;
                this->erase(this->begin() + frameno,
                            this->begin() + frameno - offset);
            }
        }
    };
    
    ControlData Cdata;
    
public:
    class SaveState
    {
    protected:
        /* CPU data */
        uint16_t regPC;
        uint8_t regA, regP, regX, regY, regS;
        char RAM[0x800];
        /* PPU data */
        char ntaRAM[0x1000];
        char palRAM[0x20];
        char spRAM[0x100];
        char ppu[0x04];
        /* Joystick data */
        uint16_t joy_readbit;
        uint32_t joy;
    public:
    } State;

public:
    Movie()
    {
        FCEUversion = 0;
        std::fill(MD5sum,MD5sum+16,0);
    }
    Movie(const std::vector<unsigned char>& data)
    {
        if(!Read(data))
        {
            fprintf(stderr, "Error: Unrecognized movie file\n");
        }
    }
    
    void DumpStatus()
    {
#if 0
        /* Locale support is a MESS, and mingw32 doesn't support wcout at all. */
        std::setlocale(LC_ALL, "");
        std::string lname = std::setlocale(LC_CTYPE, NULL);
        if(lname == "C" || lname == "POSIX") lname = "en_US.UTF-8";
        std::locale loc(lname.c_str());
        std::locale::global(loc);
#endif

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

    void ApplyDelay(unsigned frameno, long offset)
    {
        Cdata.ApplyDelay(frameno, offset);
        FrameCount += offset;
    }
    
    bool Read(const std::vector<unsigned char>& data)
    {
        return
            (reinterpret_cast<FamtasiaMovie<Movie>*>(this))->Load(data)
        ||  (reinterpret_cast<FCEUMovie<Movie>*>(this))->Load(data)
        ||  (reinterpret_cast<VirtuaNESMovie<Movie>*>(this))->Load(data)
        ||  (reinterpret_cast<NintendulatorMovie<Movie>*>(this))->Load(data)
        ||  (reinterpret_cast<NesticleMovie<Movie>*>(this))->Load(data);
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

static const std::string GetExt(const std::string& fn)
{
    std::string result;
    unsigned b=fn.size();
    while(b > 0 && fn[--b] != '.')
        result.insert(0, 1, std::tolower(fn[b]));
    return result;
}

struct DelayData
{
    unsigned frameno;
    long length;
public:
    DelayData(unsigned f, long l) : frameno(f), length(l) { }
public:
    bool operator< (const DelayData& b) const
    {
        return frameno < b.frameno;
    }
};

class MovieConfigurator
{
public:
    MovieConfigurator(Movie& movie, const std::string& var, const std::string& value)
    {
        if(var == "pal")
        {
            movie.PAL = strtol(value.c_str(),NULL,10);
        }
        else if(var == "recordcount")
        {
            movie.RecordCount = strtol(value.c_str(),NULL,10);
        }
        else if(var == "fceuver")
        {
            movie.FCEUversion = strtol(value.c_str(),NULL,10);
        }
        else if(var == "emuname")
        {
            movie.EmuName = MbToWs(value);
        }
        else if(var == "moviename")
        {
            movie.MovieName = MbToWs(value);
        }
        else if(var == "romname")
        {
            movie.ROMName = MbToWs(value);
        }
        else if(var == "md5")
        {
            bool ok_md5 = false;
            if(value.size() == 16)
            {
                ok_md5 = true;
                for(unsigned a=0; a<16; ++a)
                {
                    int v = -1, n=0;
                    std::sscanf(value.substr(a*2,2).c_str(), "%x%n", &v, &n);
                    movie.MD5sum[a] = v;
                    if(n != 2) { ok_md5 = false; break; }
                }
            }
            if(!ok_md5)
            {
                FILE* fp = std::fopen(value.c_str(), "rb");
                if(!fp)
                    perror(value.c_str());
                else
                {
                    std::fseek(fp,0,SEEK_END);
                    long skip=0, size=ftell(fp);
                    char header[16];
                    
                    std::rewind(fp);
                    std::fread(&header,1,16,fp);
                    
                    /* NES files: skip 16 bytes, read upto ROM end, ignore trailing garbage */
                    /* FDS files: ?? */
                    /* Other files: do all */

                    if(std::string(header,header+4)=="NES\032")
                    {
                        skip = 16;
                        size = (size-16) & ~511;
                    }
                    
                    std::fseek(fp, skip, SEEK_SET);
                    char*buf = new char[size];
                    std::fread(buf,1,size, fp);
                    std::string tmp = MD5sum(std::string(buf,buf+size));
                    for(unsigned a=0; a<16; ++a)
                        movie.MD5sum[a] = tmp[a];
                    delete[] buf;
                    
                    std::fclose(fp);
                }
            }
        }
        else
        {
            std::cerr << "nesmock: warning: `" << var << "' is not recognized as a var name for the -s option\n";
        }
    }
};

int main(int argc, char** argv)
{
    std::vector<DelayData> delays;
    std::map<std::string, std::string> sets;
    
    for(;;)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            {"help",     0, 0,'h'},
            {"version",  0, 0,'V'},
            {"offset",   1, 0,'o'},
            {"set",      1, 0,'s'},
            {0,0,0,0}
        };
        int c = getopt_long(argc, argv, "hVo:s:", long_options, &option_index);
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
                    "nesmock v"VERSION" - Copyright (C) 1992,2005 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "\n"
                    "Usage: nesmock [<options>] <inputfile> <outputfile>\n"
                    "\n"
                    "Transforms NES movie files to different formats.\n"
                    " --help, -h     This help\n"
                    " --offset, -o   Insert delay at <frame>,<length>\n"
                    "                Example usage: -o 14501:1\n"
                    "                Delay length may also be negative, in which case\n"
                    "                existing frames are deleted instead of copied.\n"
                    "                Frame numbers are relative to the original movie.\n"
                    "                Short syntax -o 10 uses frame number 0 by default.\n"
                    " --set, -s      Set <var>=<value>\n"
                    "                Note: All movie formats don't support all values\n"
                    "                      or may have length/type limitations.\n"
                    "                Available vars:\n"
                    "                      PAL         0 or 1\n"
                    "                      RecordCount 0..n\n"
                    "                      EmuName     string\n"
                    "                      MovieName   string\n"
                    "                      ROMName     string\n"
                    "                      MD5         string (can be a 32-char hex or a filename)\n"
                    "                      FCEUver     0..n (example: 9812 (=0.98.12))\n"
                    "                Example usage: -spal=0 -srom=\"Mario Bros.nes\" -smd5=mario.nes\n"
                    " --version, -V  Displays version information\n"
                    "\n"
                    "Supported formats:\n"
                    "  FMV  (Famtasia 5.1)        - Read & Write\n"
                    "  FCM  (FCEU 0.98.12)        - Read & Write\n"
                    "  NMV  (Nintendulator 0.950) - Read & Write\n"
                    "  VMV  (VirtuaNES)           - Read\n"
                 // "  NSM  (Nesticle)            - none\n"
                    "\n"
                    "Example:\n"
                    "  nesmock -o2 smb1a.fcm smb1a.fmv\n"
                    "\n";
                return 0;
            }
            case 'o':
            {
                char* arg = optarg;
                long frame  = strtol(arg, &arg, 10);
                long length = 0;
                bool error = false;
                if(*arg == ':')
                {
                    if(frame < 0) error = true;
                    length = strtol(arg+1, &arg, 10);
                    if(length == 0 || *arg) error = true;
                }
                else
                {
                    length = frame; frame = 0;
                    if(*arg) error = true;
                }
                if(error)
                    std::cerr << "nesmock: warning: `" << optarg << "' is not a valid parameter to -o option\n";
                
                delays.push_back(DelayData(frame, length));
                break;
            }
            case 's':
            {
                char* arg = optarg;
                std::string var, value;
                while(*arg != '\0' && *arg != '=')
                    var += std::tolower(*arg++);
                if(*arg != '=')
                {
                    std::cerr << "nesmock: warning: `" << optarg << "' is not a valid parameter to -s option\n";
                    break;
                }
                while(*++arg != '\0')
                    value += *arg;
                // TODO: Warn if setting same var twice
                sets[var] = value;
                break;
            }
        }
    }
    if(argc != optind+2)
    {
        std::cerr << "nesmock: Invalid parameters. Try `nesmock --help'\n";
        return 1;
    }
    
    std::sort(delays.begin(), delays.end());
    
    const std::string fn1 = argv[optind+0];
    const std::string fn2 = argv[optind+1];
    
    Movie movie(LoadFile(fn1));
    for(std::map<std::string, std::string>::const_iterator
        i = sets.begin(); i != sets.end(); ++i)
    {
        MovieConfigurator(movie, i->first, i->second);
    }

    movie.DumpStatus();
    
    for(unsigned a=0; a<delays.size(); ++a)
        movie.ApplyDelay(delays[a].frameno, delays[a].length);
    
    std::string ext = GetExt(fn2);
    if(ext == "fmv")
    {
        std::vector<unsigned char> file2;
        (reinterpret_cast<FamtasiaMovie<Movie>&>(movie)).Write(file2);
        WriteFile(file2, fn2);
    }
    else if(ext == "fcm")
    {
        std::vector<unsigned char> file2;
        (reinterpret_cast<FCEUMovie<Movie>&>(movie)).Write(file2);
        WriteFile(file2, fn2);
    }
    else if(ext == "nmv")
    {
        std::vector<unsigned char> file2;
        (reinterpret_cast<NintendulatorMovie<Movie>&>(movie)).Write(file2);
        WriteFile(file2, fn2);
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
