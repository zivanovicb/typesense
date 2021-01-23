#include <string>
#include <unistd.h>
#include <execinfo.h>
#include <regex>

class StackPrinter {
public:

    static std::string getexepath() {
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        return std::string(result, (count > 0) ? count : 0);
    }

    static std::string sh(const std::string& cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) throw std::runtime_error("popen() failed!");

        size_t bt_num = 0;

        std::string optional_space;
        #if __linux__
            optional_space = " ";
        #endif

        while (!feof(pipe.get())) {
            if (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
                if(bt_num++ % 2 == 1) {
                    result += optional_space + buffer.data();
                } else {
                    result += buffer.data();
                }
            }
        }
        return result;
    }

    static void bt_sighandler(int sig) {
        LOG(ERROR) << "Typesense crashed. Generating stack trace...";

        void *bt[1024];
        int bt_size;
        char **bt_syms;
        int i;

        bt_size = backtrace(bt, 1024);
        bt_syms = backtrace_symbols(bt, bt_size);

        std::regex linux_address_re("\\[(.+)\\]");
        std::string addrs;

        for (i = 1; i < bt_size; i++) {
            std::string sym = bt_syms[i];

            #if __linux__
                std::smatch matches;
                if (std::regex_search(sym, matches, linux_address_re)) {
                    std::string addr = matches[1];
                    addrs += " " + addr;
                }
            #elif __APPLE__
                std::vector<std::string> sym_parts;
                StringUtils::split(sym, sym_parts, " ");
                addrs += " " + (sym_parts.size() > 2 ? sym_parts[2] : "");
            #else
                LOG(ERROR) << sym;
            #endif
        }

        #if __linux__
            std::string command = std::string("addr2line -e ") + getexepath() + " -f -C " + addrs;
            LOG(ERROR) << sh(command);
        #elif __APPLE__
            std::string command = std::string("atos -p ") + std::to_string(getpid()) + " " + addrs;
            LOG(ERROR) << sh(command);
        #endif

        free(bt_syms);
        exit(1);
    }
};