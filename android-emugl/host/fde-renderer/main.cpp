#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

extern "C" {

extern int android_initOpenglesEmulation(void);
extern int android_startOpenglesRenderer(int width, int height,
                                         int* glesMajorVersion_out,
                                         int* glesMinorVersion_out);
extern void android_stopOpenglesRenderer(bool wait);
}

static bool terminal = false;

static void SignalTerminal(int32_t signum) {
  printf("signal %d\n", signum);
  if (signum == SIGTERM || signum == SIGINT) {
    terminal = true;
  }
}

int main(int argc, char* argv[]) {
  struct sigaction terminal_action;
  terminal_action.sa_handler = SignalTerminal;
  sigemptyset(&terminal_action.sa_mask);
  terminal_action.sa_flags = SA_RESETHAND;
  if (sigaction(SIGINT, &terminal_action, NULL) != 0) {
    syslog(LOG_ERR, "Failed to register SIGINT: %s\n", strerror(errno));
    return -2;
  }
  if (sigaction(SIGTERM, &terminal_action, NULL) != 0) {
    syslog(LOG_ERR, "Failed to register SIGTERM: %s\n", strerror(errno));
    return -2;
  }

  setenv("ANDROID_EMU_HEADLESS", "1", 0);
  /* Resolve the functions */
  if (android_initOpenglesEmulation() != 0) {
    syslog(LOG_ERR, "Failed to initialize Opengles Emulation\n");
    return -3;
  }
  syslog(LOG_DEBUG, " Start OpenGLES renderer.");
  int gles_major_version = 2;
  int gles_minor_version = 0;
  if (android_startOpenglesRenderer(0, 0, &gles_major_version,
                                    &gles_minor_version) != 0) {
    syslog(LOG_ERR, "Failed to start Opengles Renderer\n");
    return -5;
  }
  syslog(LOG_DEBUG, " gles_major_version = %d", gles_major_version);
  syslog(LOG_DEBUG, " gles_minor_version = %d", gles_minor_version);
  while (!terminal) {
    pause();
  }
  android_stopOpenglesRenderer(true);
  syslog(LOG_DEBUG, " End OpenGLES renderer.");
  printf("end\n");
  return 0;
}
