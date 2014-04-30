#ifndef _OPTIONS_H
#define _OPTIONS_H 

#include <string>
#include <map>
#include <type_traits>

class OptionManager {
    public:
        static OptionManager* get(int argc, char *argv[]);
        std::string get(std::string opt);

    private:
        static OptionManager* _instance;
        std::string _progName;
        std::map<std::string, std::string> _opts;

        OptionManager();
        void parse(int argc, char *argv[]);
        void usage();
};


#endif
