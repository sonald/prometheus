#include <unistd.h>
#include <getopt.h>
#include <iostream>

using namespace std;
#include "options.h"

OptionManager* OptionManager::_instance = nullptr;

OptionManager::OptionManager()
{
}

void OptionManager::usage()
{
    cerr << "usage: " << _progName << " [-m [text|scene] [-t theme]] [-h]" << endl;
    exit(EXIT_FAILURE);
}

void OptionManager::parse(int argc, char *argv[]) 
{
    _progName = {argv[0]};
    if (_progName.find("./") == 0) {
        _progName.erase(_progName.begin(), _progName.begin()+1);
    }

    struct option opts[] = {
        {"mode", 1, NULL, 0},
        {"theme", 1, NULL, 0},
        {NULL, 0, NULL, 0}
    };

    int c, index;
    while ((c = getopt_long(argc, argv, "m:t:h", opts, &index)) != -1) {
        switch(c) {
            case 'm': _opts["mode"] = {optarg}; break;
            case 't': _opts["theme"] = {optarg}; break;
            case 'h': usage(); break;
            default: break;
        }
    }

}

OptionManager* OptionManager::get(int argc, char *argv[])
{
    if (!_instance) {
        _instance = new OptionManager;
        _instance->parse(argc, argv);
        
    }

    return _instance;
}

std::string OptionManager::get(std::string opt)
{
    return _opts[opt];
}

