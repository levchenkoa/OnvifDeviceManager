#include "../gst/gstrtspplayer.h"
#include "../gst/onvifinitstaticplugins.h"
#include "clogger.h"

struct PlayerAndButtons {
    GstRtspPlayer * player;
    GtkWidget * play_btn;
    GtkWidget * stop_btn;
};

static void delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data) {
    gtk_main_quit();
}

void stopped_stream(GstRtspPlayer * player, struct PlayerAndButtons * data){
    C_WARN("Stream stopped callback\n");
    gtk_widget_set_sensitive(GTK_WIDGET(data->play_btn),TRUE);
}

void start_stream(GstRtspPlayer * player, struct PlayerAndButtons * data){
    C_WARN("Stream started callback\n");
    gtk_widget_set_sensitive(GTK_WIDGET(data->stop_btn),TRUE);
}


static void state_btn_cb (GtkButton *button, struct PlayerAndButtons * data) {
    if(strcmp(gtk_button_get_label(button),"Play") == 0){
        gtk_widget_set_sensitive(GTK_WIDGET(data->play_btn),FALSE);
        GstRtspPlayer__play(data->player); //Causes big hang. should be called asynchroniously
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(data->stop_btn),FALSE);
        GstRtspPlayer__stop(data->player); //Causes minimal hang. should be called asynchroniously
    }
}

int main(int argc, char *argv[])
{
    /* Initialize GTK */
    gtk_init (&argc, &argv);

    /* Initialize GStreamer */
    gst_init (&argc, &argv);
    onvif_init_static_plugins();

    GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (delete_event_cb), NULL);
    gtk_window_set_title (GTK_WINDOW (window), "OnvifMgrDevice Widget demo");

    GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_container_add(GTK_CONTAINER(window),vbox);

    struct PlayerAndButtons data;

    data.play_btn = gtk_button_new_with_label("Play");
    data.stop_btn = gtk_button_new_with_label("Stop");
    gtk_widget_set_sensitive(GTK_WIDGET(data.stop_btn),FALSE);

    data.player = GstRtspPlayer__new();
    GstRtspPlayer__set_playback_url(data.player,"rtsp://61.216.97.157:16887/stream1");
    GstRtspPlayer__set_credentials(data.player,"demo","demo");
    g_signal_connect (G_OBJECT(data.player), "stopped", G_CALLBACK (stopped_stream), &data);
    g_signal_connect (G_OBJECT(data.player), "started", G_CALLBACK (start_stream), &data);
    gtk_box_pack_start(GTK_BOX(vbox), GstRtspPlayer__createCanvas(data.player), TRUE, FALSE, 0);

    GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 0);

    g_signal_connect (G_OBJECT(data.play_btn), "clicked", G_CALLBACK (state_btn_cb), &data);
    gtk_widget_set_halign(data.play_btn, GTK_ALIGN_END);
    gtk_widget_set_valign(data.play_btn, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), data.play_btn, TRUE, FALSE, 0);

    g_signal_connect (G_OBJECT(data.stop_btn), "clicked", G_CALLBACK (state_btn_cb), &data);
    gtk_widget_set_halign(data.stop_btn, GTK_ALIGN_START);
    gtk_widget_set_valign(data.stop_btn, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), data.stop_btn, TRUE, FALSE, 0);

    gtk_window_set_default_size(GTK_WINDOW(window),100,100);
    gtk_widget_show_all (window);

    /* Start the GTK main loop. We will not regain control until gtk_main_quit is called. */
    gtk_main ();

    gst_deinit ();
}