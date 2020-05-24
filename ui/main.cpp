#include <iostream>
#include <regex>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

static void usage(const std::string &program)
{
    std::cout << "Usage: " << program << " [fbdev]" << std::endl;
    std::cout << std::endl;
    std::cout << "\tfbdev\tValid frame buffer device (e.g. /dev/fb0)" << std::endl;
}

int main(int argc, char *argv[])
{
    Display *display = nullptr; 
    if (argc < 2) {
        std::cerr << "No fb device passed. Using default." << std::endl;
        display = XOpenDisplay(nullptr);
    } else if (argc == 2) {
        std::string fb_dev(argv[1]);
        std::cerr << "fb device " << fb_dev << " passed." << std::endl;
        std::regex fb_regex("/dev/fb([0-9]+)");
        std::smatch match;
        if (std::regex_match(fb_dev, match, fb_regex)) {
            if (match.size() > 1) {
                std::ssub_match sub_match = match[1];
                std::string display_name = ":" + sub_match.str();
                display = XOpenDisplay(display_name.c_str());
            }
        }
    }

	if (!display) {
		std::cerr << "Failed to open X Display." << std::endl;
        usage(argv[0]);
		return 1;
	}

	static int visual_attribs[] = {
		GLX_X_RENDERABLE	, True,
		GLX_DRAWABLE_TYPE	, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE		, GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE	, GLX_TRUE_COLOR,
		GLX_RED_SIZE		, 8,
		GLX_GREEN_SIZE		, 8,
		GLX_BLUE_SIZE		, 8,
		GLX_ALPHA_SIZE		, 8,
		GLX_DEPTH_SIZE		, 24,
		GLX_STENCIL_SIZE	, 8,
		GLX_DOUBLEBUFFER	, True,
		None
	};

	int glx_major, glx_minor;
	if (!glXQueryVersion(display, &glx_major, &glx_minor)) {
        std::cerr << "Invalid GLX version." << std::endl;
        usage(argv[0]);
		return 1;
	}

	std::cout << "GLX Version: " << glx_major << "." << glx_minor << std::endl;

	int fbcount;
	GLXFBConfig *fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
	if (!fbc) {
		std::cerr << "Failed to retrieve framebuffer config." << std::endl;
		return 1;
	}
    std::cout << "Found " << fbcount << " matching framebuffer configs." << std::endl;

	for (int i = 0; i < fbcount; i++) {
		XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[i]);

		if (vi) {
			
		}

		XFree(vi);
	}

	XFree(fbc);

    XCloseDisplay(display);

    return 0;
}
