template<typename SaveState>
class NesticleSavestate: public SaveState
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
class NesticleMovie: public Movie
{
    // don't add member vars here.
public:
    bool Load(const std::vector<unsigned char>& data)
    {
        return false;
    }

    void Write(std::vector<unsigned char>& data)
    {
    }
};
