#include <Windows.h>
#include "structure/render/gui.hpp"

namespace FrameWork {
Options g_Options = {};
}

int main() {
    FrameWork::ApplyDarkTheme();
    FrameWork::RenderGui();
    return 0;
}
