#pragma once

#ifndef	CommonIncludes
#define CommonIncludes

#include <iostream>
#include <cstdint>
#include <iomanip>
#include <random>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#define ONE_K 1024
#define ONE_M 1024 * 1024

namespace nDebug
{
	void LogInfo (const std::string &s)	{
		std::cout << s << "\n";
	}	
	void LogInfo (const std::string &s, int val) {
		std::cout << s << val << "\n";
	}
	void LogInfo (const std::string &s0, int val0, const std::string &s1) {
		std::cout << s0 << val0 << s1 << "\n";
	}
	void LogInfo (const std::string &s0, int val0, const std::string &s1, int val1)	{
		std::cout << s0 << val0 << s1 << val1 << "\n";
	}
	void LogInfo (const std::string &s0, int val0, const std::string &s1, int val1, const std::string &s2)	{
		std::cout << s0 << val0 << s1 << val1 << s2 << "\n";
	}

	void LogError (const std::string &s)	{
		std::cerr << s << "\n";
	}

	void LogValue (const std::string &regname, uint32_t reg)	{
		std::cout << regname << ":\t0x" << std::setfill('0') << std::setw(8) << std::hex << reg << "\n";
	}
	
	std::string ConvertToString (const std::string &s1, int val, const std::string &s2) {
	    std::stringstream ss;
        ss << s1 << std::hex << std::uppercase << val << s2;
        return ss.str();
	}
}

#endif