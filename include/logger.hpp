#ifndef LOGGER_H_
#define LOGGER_H_

// Standard headers
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>

// Color constants
// TODO: refactor to kobra
#define MC_RESET	"\033[0m"
#define MC_RED		"\033[31m"
#define MC_GREEN	"\033[32m"
#define MC_YELLOW	"\033[33m"
#define MC_BLUE		"\033[34m"
#define MC_MAGENTA	"\033[35m"

// Color structs
struct log_ok {};
struct log_warn {};
struct log_error {};
struct log_reset {};
struct log_notify {};
struct log_fatal_error {};

// Overload printing these structs
std::ostream &operator<<(std::ostream &, const log_ok &);
std::ostream &operator<<(std::ostream &, const log_warn &);
std::ostream &operator<<(std::ostream &, const log_error &);
std::ostream &operator<<(std::ostream &, const log_reset &);
std::ostream &operator<<(std::ostream &, const log_notify &);
std::ostream &operator<<(std::ostream &, const log_fatal_error &);

// Logger class
class Logger {
	// Static members
	static std::ostream *	os;
	static bool		console; // Whether the stream is console (for colors)
	static std::mutex	mtx;

	// For main stream
	static std::ostream &_main_fatal_error() {
		mtx.lock();
		if (console) *os << log_fatal_error();
		*os << "[KOBRA: " << time() << ": FATAL ERROR] ";
		if (console) *os << log_reset();
		if (!console) os->flush();
		mtx.unlock();
		return *os;
	}

	static std::ostream &_main_plain() {
		if (!console) os->flush();
		return *os;
	}

	static std::ostream &_main_ok() {
		mtx.lock();
		if (console) *os << log_ok();
		*os << "[KOBRA: " << time() << "] ";
		if (console) *os << log_reset();
		if (!console) os->flush();
		mtx.unlock();
		return *os;
	}

	static std::ostream &_main_error() {
		mtx.lock();
		if (console) *os << log_error();
		*os << "[KOBRA: " << time() << "] ";
		if (console) *os << log_reset();
		if (!console) os->flush();
		mtx.unlock();
		return *os;
	}

	static std::ostream &_main_warn() {
		mtx.lock();
		if (console) *os << log_warn();
		*os << "[KOBRA: " << time() << "] ";
		if (console) *os << log_reset();
		if (!console) os->flush();
		mtx.unlock();
		return *os;
	}

	static std::ostream &_main_notify() {
		mtx.lock();
		if (console) *os << log_notify();
		*os << "[KOBRA: " << time() << "] ";
		if (console) *os << log_reset();
		if (!console) os->flush();
		mtx.unlock();
		return *os;
	}
public:
	static std::string time() {
		auto t = std::time(0);
		auto tm = *std::localtime(&t);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%m/%d/%Y %H:%M:%S");
		return oss.str();
	}

	// For main stream
	static std::ostream &plain() {
		return _main_plain();
	}

	static std::ostream &ok() {
		return _main_ok();
	}

	static std::ostream &error() {
		return _main_error();
	}

	static std::ostream &warn() {
		return _main_warn();
	}

	static std::ostream &notify() {
		return _main_notify();
	}

	// C-string overloads
	static void ok(const char *msg) {
		ok() << msg << std::endl;
	}

	static void error(const char *msg) {
		error() << msg << std::endl;
	}

	static void warn(const char *msg) {
		warn() << msg << std::endl;
	}

	static void notify(const char *msg) {
		notify() << msg << std::endl;
	}

	static void fatal_error(const char *msg) {
		_main_fatal_error() << msg << std::endl;
		exit(-1);
	}

	// C++ string overloads
	static void ok(const std::string &msg) {
		ok(msg.c_str());
	}

	static void error(const std::string &msg) {
		error(msg.c_str());
	}

	static void warn(const std::string &msg) {
		warn(msg.c_str());
	}

	static void notify(const std::string &msg) {
		notify(msg.c_str());
	}

	// With bracketed origin
	static void ok(const char *origin, const char *msg) {
		ok() << "[" << origin << "] " << msg << std::endl;
	}

	static void error(const char *origin, const char *msg) {
		error() << "[" << origin << "] " << msg << std::endl;
	}

	static void warn(const char *origin, const char *msg) {
		warn() << "[" << origin << "] " << msg << std::endl;
	}

	static void notify(const char *origin, const char *msg) {
		notify() << "[" << origin << "] " << msg << std::endl;
	}

	// Bracketed origin and stream
	static std::ostream &ok_from(const char *origin) {
		return ok() << "[" << origin << "] ";
	}

	static std::ostream &error_from(const char *origin) {
		return error() << "[" << origin << "] ";
	}

	static std::ostream &warn_from(const char *origin) {
		return warn() << "[" << origin << "] ";
	}

	static std::ostream &notify_from(const char *origin) {
		return notify() << "[" << origin << "] ";
	}

	// Cached logging
	static void error_cached(const std::string &msg) {
		static std::set <std::string> cached;

		if (cached.find(msg) == cached.end()) {
			cached.insert(cached.end(), msg);
			error(msg);
		}
	}

	static void warn_cached(const std::string &msg) {
		static std::set <std::string> cached;

		if (cached.find(msg) == cached.end()) {
			cached.insert(cached.end(), msg);
			warn(msg);
		}
	}

	// Switching streams
	static void switch_console(std::ostream &new_os) {
		// Delete the old stream if not console
		if (!console) delete os;

		// Assumes that new_os is a standard IO stream
		os = &new_os;
		console = true;
	}

	static void switch_file(const std::string &file) {
		// Delete the old stream if not console
		if (!console) delete os;

		// Create ofstream at file
		os = new std::ofstream(file);
		console = false;
	}
};

// Macros for logging
// TODO: option to swthich function and pretty function
#define KOBRA_LOG_FUNC(type) Logger::type##_from(__FUNCTION__)

#define LINE_TO_STRING(line) #line
#define LINE_TO_STRING2(line) LINE_TO_STRING(line)

#define KOBRA_LOG_FILE(type) Logger::type##_from(__FILE__ ": " LINE_TO_STRING2(__LINE__))

#define KOBRA_ASSERT(cond, msg)					\
	if (!(cond)) {						\
		KOBRA_LOG_FUNC(error) << msg << std::endl;	\
		throw std::runtime_error(msg);			\
	}

#endif
