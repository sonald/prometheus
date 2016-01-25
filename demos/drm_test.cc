/**
 * used to test drm api
 *  g++ -std=c++1y `pkg-config libdrm --libs --cflags ` drm_test.cc
 */

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>

#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

static int dfd = -1;

int main(int argc, char *argv[])
{
    if (!drmAvailable()) {
        cerr << "drm not available" << endl;
        return -1;
    }

    dfd = drmOpen("i915", NULL);
    if (dfd < 0) {
        cerr << "drmOpen failed, try raw open" << endl;
        dfd = open("/dev/dri/card0", O_RDWR);
        if (dfd <= 0) {
            cerr << "drm open failed" << endl;
            return -1;
        }
    }

    float fps = 0.0;
    vector<int> intervals;

    drmVBlank vblk;

    vblk.request.sequence = 1;
    vblk.request.type = DRM_VBLANK_RELATIVE;

    struct timeval tv = {0};
    bool quit = false;
    int limit  = 100;
    while (!quit && limit-- > 0) {
        int ret = drmWaitVBlank(dfd, &vblk);

        int old = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        int sec = vblk.reply.tval_sec * 1000 + vblk.reply.tval_usec/1000;

        cerr << "drmWaitVBlank reply: seq " << vblk.reply.sequence 
            << ", interval: " <<  sec - old
            << endl;

        vblk.request.type = DRM_VBLANK_RELATIVE;
        vblk.request.sequence = 1;

        tv.tv_sec = vblk.reply.tval_sec;
        tv.tv_usec = vblk.reply.tval_usec;

        if (old) intervals.emplace_back(sec - old);
    }

    auto sum = std::accumulate(intervals.begin(), intervals.end(), 0);
    fps = 1000.0f * intervals.size() / sum;
    cerr << "estimated fps: " << fps << endl;

    drmClose(dfd);

    return 0;
}
