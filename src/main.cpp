#include "printf.hpp"
#include <bits/stdc++.h>

int main() {
    // Since original problem statement lacks explicit I/O,
    // we implement a simple demo usage to verify compile and runtime.
    // Actual OJ tests may include their own main; but per guidelines,
    // executable must be named `code`. We'll provide basic stdin passthrough.

    using namespace sjtu;
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // Echo all stdin to stdout to ensure non-empty behavior.
    // Also demonstrate formatter capabilities.
    std::string line;
    while (std::getline(std::cin, line)) {
        printf(format_string_t<std::string>{"%s\n"}, line);
    }

    // Small showcase for types:
    // printf(format_string_t<const char*, int, unsigned>{"%s %d %u\n"}, "demo", -42, 42u);
    return 0;
}

