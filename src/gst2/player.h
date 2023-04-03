#include "gst/gst.h"
#include "onvif_list.h"
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/video/video.h>
#include <gdk/gdk.h>
#include "overlay.h"
#include "../gui/credentials_input.h"
#include "../device_list.h"

#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif


#ifndef ONVIF_PLAYER_H_ 
#define ONVIF_PLAYER_H_

/* Structure to contain all our information, so we can pass it around */
typedef struct _OnvifPlayer OnvifPlayer;

typedef struct _OnvifPlayer {
  GstElement *pipeline; /* Our one and only pipeline */
  GstElement *backpipe;
  GstElement *src;  /* RtspSrc to support backchannel */
  GstElement *sink;  /* Video Sink */
  GstElement *mic_volume_element;
  int pad_found;
  GstElement * video_bin;
  int no_video;
  int video_done;
  GstElement * audio_bin;
  int no_audio;
  int audio_done;
  GstVideoOverlay *overlay; //Overlay rendered on the canvas widget
  OverlayState *overlay_state;

  int retry;
  int playing;
  void (*retry_callback)(OnvifPlayer *, void * user_data);
  void * retry_user_data;

  Device* device; /* Currently selected device */
  DeviceList *device_list;
  GtkWidget *listbox;
  GtkWidget *details_notebook;
  GtkWidget *canvas_handle;
  GtkWidget *canvas;
  GtkWidget *loading_handle;
  gdouble level; //Used to calculate level decay
  guint back_stream_id;
  CredentialsDialog * dialog;

  pthread_mutex_t * player_lock;
} OnvifPlayer;

OnvifPlayer * OnvifPlayer__create();  // equivalent to "new Point(x, y)"
void OnvifPlayer__destroy(OnvifPlayer* self);  // equivalent to "delete point"
void OnvifPlayer__set_retry_callback(OnvifPlayer* self, void (*retry_callback)(OnvifPlayer *, void *), void * user_data);
void OnvifPlayer__set_playback_url(OnvifPlayer* self, char *url);
void OnvifPlayer__stop(OnvifPlayer* self);
void OnvifPlayer__play(OnvifPlayer* self);

/*
Compared to play, retry is design to work after a stream failure.
Stopping will essentially break the retry method and stop the loop.
*/
void OnvifPlayer__retry(OnvifPlayer* self);
GtkWidget * OnvifDevice__createCanvas(OnvifPlayer *self);
gboolean OnvifPlayer__is_mic_mute(OnvifPlayer* self);
void OnvifPlayer__mic_mute(OnvifPlayer* self, gboolean mute);

#endif