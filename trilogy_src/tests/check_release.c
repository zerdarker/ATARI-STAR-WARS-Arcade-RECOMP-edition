/* Checks update(): latest release, version compare, download (with GitHub's
 * redirect), zip.  Pointed at a public repo whose zips have no
 * AtariStarWarsArcadeRE.exe, so it must end in "UPDATE FAILED" and replace nothing.
 * Build and run: build_check_release.bat */
#define REPO "cli/cli"
#include "../../selector_src/selector.c"

int main(void)
{
    int r = update();
    printf("update() = %d, status: %s\n", r, status_msg);
    return !(r == 0 && !strcmp(status_msg, "UPDATE FAILED"));
}
