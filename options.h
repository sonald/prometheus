#ifndef _OPTIONS_H
#define _OPTIONS_H 

#include <type_traits>

class OptionManager {
    public:
        static OptionManager* get(int argc, char *argv[]);
        template <typename T>
        T get(string opt);

    private:
        static OptionManager* _instance;
        string _progName;
        string _mode;

        OptionManager();
        void parse(int argc, char *argv[]);
        void usage();
};


template <typename T> 
T OptionManager::get(string opt)
{
    if (opt == "mode")
        return _mode;

    return "";
}

#endif
