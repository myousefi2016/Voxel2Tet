#include <cstdlib>

#include "Options.h"

namespace voxel2tet
{

Options :: Options( int argc, char *argv[], ValueMap DefaultValues )
{
    // Parse command line arguments and simply store them in a map
    for (int i=1; i<argc; i++) {
        if (char(argv[i][0]) == '-') {
            bool NextIsFlag = false;

            if (i==(argc-1)) {
                NextIsFlag = true;
            } else if (char(argv[i+1][0]) == '-') {
                NextIsFlag = true;
            }

            std :: string FlagName;
            int j=1;
            while (argv[i][j]!='\0') {
                FlagName += argv[i][j];
                j++;
            }

            std :: string Value;
            if (!NextIsFlag) {
                Value = argv[i+1];
            } else {
                Value = "1";
            }
            this->OptionMap[FlagName] = Value;

            // std :: cout << "FlagName: " << FlagName << ", Value:" << Value <<"\n";
        }
    }
    this->DefaultMap = DefaultValues;
}

bool Options :: has_key(std::string keyname)
{
    if (this->OptionMap.find(keyname) == this->OptionMap.end()) {
        return false;
    }
    return true;
}

std::string Options :: GiveStringValue(std::string keyname)
{
    std::string Value;
    if (this->has_key(keyname)) {
        Value = this->OptionMap[keyname];
    } else if (this->DefaultMap.find(keyname) != this->DefaultMap.end()) {
        Value = this->DefaultMap[keyname];
    }
    return Value;
}

int Options :: GiveIntegerValue(std::string keyname)
{
    std::string Value = this->GiveStringValue(keyname);
    int iValue = std::atoi (Value.c_str());
    return iValue;
}

double Options :: GiveDoubleValue(std::string keyname)
{
    std::string Value = this->GiveStringValue(keyname);
    double dValue = std::atof (Value.c_str());
    return dValue;
}

bool Options :: GiveBooleanValue(std::string keyname)
{
    std::string Value = this->GiveStringValue(keyname);
    double iValue = std::atoi (Value.c_str());
    return bool(iValue);
}

int Options :: GiveIntegerList(std::string keyname)
{
    // Not implemented
}


}