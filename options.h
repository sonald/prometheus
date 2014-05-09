#ifndef _OPTIONS_H
#define _OPTIONS_H 

#include <string>
#include <map>
#include <type_traits>

class OptionManager {
    public:
        static OptionManager* get(int argc, char *argv[]);
        std::string progName() const { return _progName; }
        template<typename T>
        T value(std::string opt);

    private:
        static OptionManager* _instance;
        std::string _progName;
        std::map<std::string, std::string> _opts;

        OptionManager();
        void parse(int argc, char *argv[]);
        void usage();

        int _value_helper(std::string, int);
        bool _value_helper(std::string, bool);
        float _value_helper(std::string, float);
        std::string _value_helper(std::string, std::string);
};

template<typename T>
T OptionManager::value(std::string opt)
{
    return _value_helper(_opts[opt], T {});
}


#endif
