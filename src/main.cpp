#include "renderer.h"

int main() {
    
    try {
        Renderer renderer;
        renderer.initRenderer();
        return EXIT_SUCCESS;
    }
    catch(const std::exception& e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}