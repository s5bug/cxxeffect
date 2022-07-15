#include <cstdio>
#include <string>

#include <eff.hpp>

eff::task<eff::top> print(std::string s) {
    return eff::delay<eff::top>([s]() {
        std::printf("%s\n", s.c_str());
        return eff::top();
    });
}

int main() {
    eff::resource<eff::dispatcher> dp = eff::make_dispatcher();
    (void) dp;

    eff::task<eff::top> ph = print("Hello, world!");
    (void) ph;
}
