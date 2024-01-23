// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>

class CommandLine
{
    std::vector<std::string> args;
    std::vector<bool> argsUsed;
    std::ostringstream help;

    void addHelp(const char * longTag, const char * shortTag, const char * description, std::string params)
    {
        if (longTag) help << longTag << params << std::endl;
        if (shortTag) help << shortTag << params << std::endl;
        help << '\t' << description << '\n' << std::endl;
    }

    void badArg(int optionIndex, std::string name, int index)
    {
        std::cerr << args[optionIndex] << ": Invalid " << name << ": " << args[index] << std::endl;
        exit(-1);
    }

    int findIndex(const char * longTag, const char * shortTag) const
    {
        auto it = std::find(begin(args), end(args), longTag);
        if (it != end(args)) return (int)(it - begin(args));
        it = std::find(begin(args), end(args), shortTag);
        return it != end(args) ? (int)(it - begin(args)) : -1;
    }

    int find(const char * longTag, const char * shortTag, int numParams)
    {
        auto index = findIndex(longTag, shortTag);
        if (index != -1)
        {
            if (index + numParams < static_cast<int>(args.size()))
            {
                for (int i = 0; i <= numParams; ++i)
                {
                    argsUsed[index + i] = true;
                }
                return index;
            }
            else
            {
                std::cerr << "Not enough arguments provided for option " << longTag;
                if (shortTag) std::cerr << " (" << shortTag << ")";
                std::cerr << std::endl;
                exit(-1);
            }
        }
        return -1;
    }

public:
    CommandLine(int argc, char * argv[])
        : args(argv + 1, argv + argc), argsUsed(args.size(), false)
    {
    }

	size_t numArgs()
	{
		return args.size();
	}

    int checkAllArgsUsed()
    {
        int status = 0;

        for (size_t i = 0; i < args.size(); ++i)
        {
            if (!argsUsed[i])
            {
                std::cerr << "Unknown option: " << args[i] << std::endl;
                status = -1;
            }
        }

        return status;
    }

    std::string getHelpMessage() const { return help.str(); }

    void addOption(bool & value, const char * longTag, const char * shortTag, const char * description)
    {
        addHelp(longTag, shortTag, description, "");
        if (find(longTag, shortTag, 0) != -1)
            value = true;
    }

    void addOption(std::string & value, std::string name, const char * longTag, const char * shortTag, const char * description)
    {
        addHelp(longTag, shortTag, description, " <" + name + ">");
        int index = find(longTag, shortTag, 1);
        if (index != -1)
            value = args[index + 1];
    }

    void addOption(int & value, std::string name, const char * longTag, const char * shortTag, const char * description)
    {
        addHelp(longTag, shortTag, description, " <" + name + ">");
        int index = find(longTag, shortTag, 1);
        if (index != -1)
        {
            if (!(std::istringstream(args[index + 1]) >> value))
                badArg(index, name, index + 1);
        }
    }

    void addOption(uint32_t & value, std::string name, const char * longTag, const char * shortTag, const char * description)
    {
        addHelp(longTag, shortTag, description, " <" + name + ">");
        int index = find(longTag, shortTag, 1);
        if (index != -1)
        {
            if (!(std::istringstream(args[index + 1]) >> value))
                badArg(index, name, index + 1);
        }
    }

    void addOption(double & value, std::string name, const char * longTag, const char * shortTag, const char * description)
    {
        addHelp(longTag, shortTag, description, " <" + name + ">");
        int index = find(longTag, shortTag, 1);
        if (index != -1)
        {
            if (!(std::istringstream(args[index + 1]) >> value))
                badArg(index, name, index + 1);
        }
    }

    void addOption(int & val1, std::string name1, int & val2, std::string name2, const char * longTag, const char * shortTag, const char * description)
    {
        addHelp(longTag, shortTag, description, " <" + name1 + "> <" + name2 + ">");
        int index = find(longTag, shortTag, 2);
        if (index != -1)
        {
            if (!(std::istringstream(args[index + 1]) >> val1))
                badArg(index, name1, index + 1);
            if (!(std::istringstream(args[index + 2]) >> val2))
                badArg(index, name2, index + 2);
        }
    }
};
