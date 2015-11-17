#define NONIUS_RUNNER
#include "nonius.h++"
#include <iostream>
#include <vector>

//////////////////////////////////////////////////////////////////////////////
//
// Create some data
//

int constexpr g_cBuffer = 1048576; // 1M
struct SBuffer : std::string {
	SBuffer() {
		for(int i=0; i<g_cBuffer; i++) {
			push_back('A' + i%26);
		}
	}
} const g_strBuffer;

int constexpr g_cBufferInts = 10000; 
struct SIntBuffer : std::vector<unsigned int> {
	SIntBuffer() {
		srand(1);
		for(int i=0; i<g_cBufferInts; i++) {
			emplace_back(rand());
		}
	}
} const g_vecnBuffer;

//////////////////////////////////////////////////////////////////////////////
//
// Benchmark iostreams
//

NONIUS_BENCHMARK("iostream - write 1M chunk", [](nonius::chronometer meter) {
	std::basic_ofstream<char> of("test_iostream_1M.txt", std::ios::binary | std::ios::trunc);

	meter.measure([&] { 
		of.write(g_strBuffer.data(), g_strBuffer.size());
	});
});

NONIUS_BENCHMARK("iostream - pipe characters", [](nonius::chronometer meter) {
	std::basic_ofstream<char> of("test_iostream_char.txt", std::ios::binary | std::ios::trunc);

	meter.measure([&] { 
		for(auto itch = g_strBuffer.begin(); itch!=g_strBuffer.end(); ++itch) {
			of << *itch;
		}
	});
});

NONIUS_BENCHMARK("iostream - formatted write", [](nonius::chronometer meter) {
	std::basic_ofstream<char> of("test_iostream_format.txt", std::ios::binary | std::ios::trunc);

	meter.measure([&] { 
		for(auto n : g_vecnBuffer) {
			of << "Dec: " << std::dec << n << " Hex: " << std::hex << n << "\n";
		}
	});
});

//////////////////////////////////////////////////////////////////////////////
//
// Benchmark FILE*
//

NONIUS_BENCHMARK("FILE* - write 1M chunk", [](nonius::chronometer meter) {
	std::FILE* f = fopen("test_FILE_1M.txt", "w");
	meter.measure([&] { 
		std::fwrite(g_strBuffer.data(), g_strBuffer.size(), 1, f);
	});	
	std::fclose(f);
});

NONIUS_BENCHMARK("FILE* - pipe characters", [](nonius::chronometer meter) {
	std::FILE* f = fopen("test_FILE_char.txt", "w");
	meter.measure([&] { 
		for(auto itch = g_strBuffer.begin(); itch!=g_strBuffer.end(); ++itch) {
			std::fputc(*itch, f);
		}
	});
	std::fclose(f);
});

NONIUS_BENCHMARK("FILE* - formatted write", [](nonius::chronometer meter) {
	std::FILE* f = fopen("test_FILE_format.txt", "w");
	meter.measure([&] { 
		for(auto n : g_vecnBuffer) {
			std::fprintf(f, "Dec: %u Hex: %x\n", n, n);
		}
	});
	std::fclose(f);
});

//////////////////////////////////////////////////////////////////////////////
//
// Platform-independent, write-only, unbuffered file
//

#ifdef __APPLE__
#include <fcntl.h>
struct appendfile {
	int m_fd;
	appendfile(char const* szName) 
		: m_fd(open(szName, O_CREAT | O_WRONLY | O_TRUNC,  S_IRUSR | S_IWUSR))
	{
		assert(-1!=m_fd);
	}

	~appendfile() {
		close(m_fd);
	}

	void write(void const* pv, std::size_t cb) {
		::write(m_fd, pv, cb);
	}
};
#elif _MSC_VER
#include <Windows.h>

struct appendfile {
	HANDLE m_hfile;
	appendfile(char const* szName, bool bTemp = false)
		: m_hfile(CreateFile(szName, FILE_APPEND_DATA, 0, nullptr, CREATE_ALWAYS, bTemp ? FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE : 0, nullptr))
	{
		assert(INVALID_HANDLE_VALUE!=m_hfile);
	}

	~appendfile() {
		CloseHandle(m_hfile);
	}

	void write(void const* pv, std::size_t cb) {
		DWORD cbWritten;
		WriteFile(m_hfile, pv, cb, &cbWritten, nullptr);
	}
};
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Pipe operator for appendfile
//

appendfile& operator<<(appendfile& os, char ch) {
	os.write(&ch, 1);
	return os;
}

appendfile& operator<<(appendfile& os, char const* pch) {
	os.write(pch, std::strlen(pch));
	return os;
}

std::string& operator<<(std::string& os, char ch) {
	os.push_back(ch);
	return os;
}

std::string& operator<<(std::string& os, char const* pch) {
	os.append(pch);
	return os;
}

//////////////////////////////////////////////////////////////////////////////
//
// Benchmark raw IO
//

NONIUS_BENCHMARK("OS file - write 1M chunk", [](nonius::chronometer meter) {
	appendfile f("test_os_1M.txt");
	meter.measure([&] { 
		f.write(g_strBuffer.data(), g_strBuffer.size());
	});	
});

// This is very, very slow (of course):
/*
NONIUS_BENCHMARK("OS file - pipe characters", [](nonius::chronometer meter) {
	appendfile f("test_os_char.txt");
	meter.measure([&] { 
		for(auto itch = g_strBuffer.begin(); itch!=g_strBuffer.end(); ++itch) {
			f << *itch;
		}
	});
});
*/

//////////////////////////////////////////////////////////////////////////////
//
// Custom formatted output that works on raw OS files too
//

template< typename T, unsigned int nWidth, char c_chLetterBase>
class as_hex_impl {
private:
	typename boost::uint_t< CHAR_BIT*sizeof(T) >::exact m_n;
public:
	as_hex_impl( T const& n ) : m_n(n) {}; // print the bit pattern of anything we get

	template<typename Sink> 
	friend Sink& operator<<( Sink& os, as_hex_impl const& n) {
		static_assert( 0<nWidth, "" );
		static_assert( nWidth<=(sizeof(n.m_n)*CHAR_BIT+3)/4, "" );

		auto nShift=sizeof(n.m_n)*CHAR_BIT;
		do {
			nShift-=4;
		} while( nWidth*4<=nShift && 0==(n.m_n>>nShift) );
		for(;;) {
			auto const nDigit=(n.m_n>>nShift)&0xf;
			os << ( nDigit<10 ? ('0' + nDigit) : (c_chLetterBase + (nDigit-10)) );
			if( 0==nShift ) break;
			nShift-=4;
		}
		return os;
	}
};

template< typename T >
auto as_unpadded_uc_hex(T const& t) { return as_hex_impl<T, 1, 'A'>(t); }

template< typename T>
struct integral_as_dec {
	T m_n;
	integral_as_dec( T n ) : m_n(n) {};

	template<typename Sink>
	friend Sink& operator<<( Sink& os, integral_as_dec<T> const& n) {
		auto ach = boost::lexical_cast<std::array<char, 50>>(n.m_n + 0/*force integral promotion, otherwise unsigned/signed char gets printed as character*/);
		os << &ach[0];
		return os;
	}
};

template<typename T>
auto as_dec(T const& t) { return integral_as_dec<T>(t); }

NONIUS_BENCHMARK("OS file - formatted write", [](nonius::chronometer meter) {
	appendfile f("test_os_format.txt");
	meter.measure([&] { 
		for(auto n : g_vecnBuffer) {
			f << "Dec: " << as_dec(n) << " Hex: " << as_unpadded_uc_hex(n) << "\n";
		}
	});
});

//////////////////////////////////////////////////////////////////////////////
//
// Compare stringstream to our own formatted output on simple strings
//

NONIUS_BENCHMARK("stringstream - formatted write", [](nonius::chronometer meter) {
	std::string str; 
	meter.measure([&] { 
		std::basic_stringstream<char> ss;
		for(auto n : g_vecnBuffer) {
			ss << "Dec: " << std::dec << n << " Hex: " << std::hex << n << "\n";
		}
		str = ss.str();
	});
	appendfile f("test_stringstream_format.txt");
	f << str.c_str();
});

NONIUS_BENCHMARK("string - formatted write", [](nonius::chronometer meter) {
	std::string str;
	meter.measure([&] {
		for(auto n : g_vecnBuffer) {
			str << "Dec: " << as_dec(n) << " Hex: " << as_unpadded_uc_hex(n) << "\n";
		}
	});
	appendfile f("test_str_format.txt");
	f << str.c_str();
});