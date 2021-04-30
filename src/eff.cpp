#include "eff.hpp"
#include "stream.hpp"
#include "task.hpp"

int main() {
    eff::stream<eff::task, char> in = eff::io::cin<eff::task>(1024);
    eff::pipe<eff::task, char, eff::bot> out = eff::io::cout<eff::task>();

    eff::stream<eff::task, eff::bot> program = in.through(out);

    eff::task<eff::top> result = program.compile().drain();

    result.unsafeRunSync();
}
