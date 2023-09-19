
#include <stddef.h>
#include <stdio.h>
#include "device.h"
#include "gui_utils.h"
#include <netdb.h>
#include <arpa/inet.h>

extern char _binary_prohibited_icon_png_size[];
extern char _binary_prohibited_icon_png_start[];
extern char _binary_prohibited_icon_png_end[];

extern char _binary_locked_icon_png_size[];
extern char _binary_locked_icon_png_start[];
extern char _binary_locked_icon_png_end[];

extern char _binary_warning_png_size[];
extern char _binary_warning_png_start[];
extern char _binary_warning_png_end[];

struct _Device {
  CObject parent;

  OnvifDevice * onvif_device;
  GtkWidget * image_handle;
  GtkWidget * profile_dropdown;

  pthread_mutex_t * ref_lock;
  int profile_index;
  int selected;

  void (*profile_callback)(Device *, void *);
  void * profile_userdata;
};

typedef struct {
    Device * device;
    OnvifProfiles * profiles;
} GUIProfileEvent;

void priv_Device__destroy(CObject * self);
void priv_Device__profile_changed(GtkComboBox* self, Device * device);
void _priv_Device__lookup_hostname(void * user_data);
void _priv_Device__load_thumbnail(void * user_data);
void _priv_Device__load_profiles(void * user_data);
gboolean * gui_Device__display_profiles (void * user_data);

void priv_Device__destroy(CObject * self){
    OnvifDevice__destroy(((Device*)self)->onvif_device);
}
int _priv_Device__lookup_hostname_netbios(Device * device, char * hostname){
    char * dev_ip = OnvifDevice__get_ip(device->onvif_device);
    printf("NetBIOS Lookup ... %s\n",dev_ip);
    int ret;
    //Lookup hostname
    struct in_addr in_a;
    inet_pton(AF_INET, dev_ip, &in_a);
    struct hostent* host;
    host = gethostbyaddr( (const void*)&in_a, 
                        sizeof(struct in_addr), 
                        AF_INET );
    if(host){
        printf("Found hostname : %s\n",host->h_name);
        hostname = host->h_name;
        ret = 1;
    } else {
        printf("Failed to get hostname ...\n");
        hostname = NULL;
    }

    printf("Retrieved hostname : %s\n",hostname);
    free(dev_ip);
    return ret;
}

int _priv_Device__lookup_hostname_dns(Device * device, char * hostname){
    int ret = 0;
    char servInfo[NI_MAXSERV];
    memset(&servInfo,0,sizeof(servInfo));

    char * dev_ip = OnvifDevice__get_ip(device->onvif_device);
    printf("DNS Lookup ... %s\n",dev_ip);

    struct sockaddr_in sa_in;
    sa_in.sin_family = AF_INET;
    sa_in.sin_addr.s_addr = inet_addr(dev_ip);
    sa_in.sin_port = htons(25);

    if (getnameinfo((struct sockaddr*) &sa_in, sizeof(struct sockaddr), hostname, sizeof(hostname),
                servInfo, NI_MAXSERV, NI_NAMEREQD)){
        printf("Failed to get hostname ...\n");
    } else {
        printf("Retrieved host=%s, serv=%s\n", hostname, servInfo);
        ret = 1;
    }


    free(dev_ip);
    return ret;
}

void _priv_Device__lookup_hostname(void * user_data){
    Device * device = (Device *) user_data;
    char hostname[NI_MAXHOST];
    memset(&hostname,0,sizeof(hostname));
    
    printf("_priv_Device__lookup_hostname\n");
    if(!CObject__addref((CObject*)device)){
        printf("WARN _priv_Device__lookup_hostname - invalid device\n");
        return;
    }

    if(!_priv_Device__lookup_hostname_dns(device,hostname)){
        _priv_Device__lookup_hostname_netbios(device,hostname);
    }

    printf("_priv_Device__lookup_hostname - done\n");
    CObject__unref((CObject*)device);
}

void _priv_Device__load_thumbnail(void * user_data){
    printf("_priv_Device__load_thumbnail\n");
    GtkWidget *image;
    GError *error = NULL;
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *scaled_pixbuf = NULL;
    double size = -1;
    char * imgdata = NULL;
    OnvifSnapshot * snapshot = NULL;
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();

    Device * device = (Device *) user_data;

    if(!CObject__addref((CObject*)device)){
        printf("WARN _priv_Device__load_thumbnail - invalid device #1\n");
        return;
    }
    
    OnvifErrorTypes oerror = OnvifDevice__get_last_error(device->onvif_device);
    if(oerror == ONVIF_ERROR_NONE){
        OnvifMediaService * media_service = OnvifDevice__get_media_service(device->onvif_device);
        snapshot = OnvifMediaService__getSnapshot(media_service,Device__get_selected_profile(device));
        if(!snapshot){
            printf("_priv_Device__load_thumbnail- Error retrieve snapshot.");
            goto warning;
        }
        imgdata = OnvifSnapshot__get_buffer(snapshot);
        size = OnvifSnapshot__get_size(snapshot);
    } else if(oerror == ONVIF_NOT_AUTHORIZED){
        imgdata = _binary_locked_icon_png_start;
        size = _binary_locked_icon_png_end - _binary_locked_icon_png_start;
    } else {
        goto warning;
    }

    //Check is device is still valid. (User performed scan before snapshot finished)
    if(!CObject__is_valid((CObject*)device)){
        printf("WARN _priv_Device__load_thumbnail - invalid device #2\n");
        goto exit;
    }

    //Attempt to get downloaded pixbuf or locked icon
    if(gdk_pixbuf_loader_write (loader, (unsigned char *)imgdata, size,&error)){
        pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    } else {
        if(error->message){
            printf("Error writing png to GtkPixbufLoader : %s\n",error->message);
        } else {
            printf("Error writing png to GtkPixbufLoader : [null]\n");
        }
    }

warning:
    //Check is device is still valid. (User performed scan before snapshot finished)
    if(!CObject__is_valid((CObject*)device)){
        printf("WARN _priv_Device__load_thumbnail - invalid device #3\n");
        goto exit;
    }

    //Show warning icon in case of failure
    if(!pixbuf){
        if(gdk_pixbuf_loader_write (loader, (unsigned char *)_binary_warning_png_start, _binary_warning_png_end - _binary_warning_png_start,&error)){
            pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
        } else {
            if(error->message){
                printf("Error writing warning png to GtkPixbufLoader : %s\n",error->message);
            } else {
                printf("Error writing warning png to GtkPixbufLoader : [null]\n");
            }
        }
    }

    //Check is device is still valid. (User performed scan before spinner showed)
    if(!CObject__is_valid((CObject*)device)){
        printf("WARN _priv_Device__load_thumbnail - invalid device #4\n");
        goto exit;
    }

    //Display pixbuf
    if(pixbuf){
        double ph = gdk_pixbuf_get_height (pixbuf);
        double pw = gdk_pixbuf_get_width (pixbuf);
        double newpw = 40 / ph * pw;
        scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,newpw,40,GDK_INTERP_NEAREST);
        image = gtk_image_new_from_pixbuf (scaled_pixbuf);

        //Check is device is still valid. (User performed scan before scale finished)
        if(!CObject__is_valid((CObject*)device)){
            printf("WARN _priv_Device__load_thumbnail - invalid device #5\n");
            goto exit;
        }

        gui_update_widget_image(image,device->image_handle);
    } else {
        printf("Failed all thumbnail creation.\n");
    }

exit:
    if(loader){
        gdk_pixbuf_loader_close(loader,NULL);
        g_object_unref(loader);
    }
    if(scaled_pixbuf){
        g_object_unref(scaled_pixbuf);
    }
    OnvifSnapshot__destroy(snapshot);

    printf("_priv_Device__load_thumbnail done.\n");
    CObject__unref((CObject*)device);
} 

void priv_Device__profile_changed(GtkComboBox* self, Device * device){
    pthread_mutex_lock(device->ref_lock);
    int new_index = gtk_combo_box_get_active(self);
    if(new_index == -1){
        new_index = 0; //Default to the first profile if nothing is available
    }
    
    if(new_index != device->profile_index){
        device->profile_index = new_index;
        if(device->profile_callback){
            device->profile_callback(device,device->profile_userdata);
        }
    }
    pthread_mutex_unlock(device->ref_lock);
}

gboolean * gui_Device__display_profiles (void * user_data){
    GUIProfileEvent * evt = (GUIProfileEvent *) user_data;
    printf("gui_Device__display_profiles\n");

    if(!CObject__addref((CObject*)evt->device)){
        printf("WARN gui_Device__display_profiles - invalid device\n");
        return FALSE;
    }

    if(OnvifDevice__get_last_error(evt->device->onvif_device) == ONVIF_NOT_AUTHORIZED){
        printf("WARN gui_Device__display_profiles - unauthorized\n");
        goto exit;
    }
    
    GtkListStore *liststore = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(evt->device->profile_dropdown)));
    for (int i = 0; i < OnvifProfiles__get_size(evt->profiles); i++){
        OnvifProfile * profile = OnvifProfiles__get_profile(evt->profiles,i);
        char * name = OnvifProfile__get_name(profile);
        char * token = OnvifProfile__get_token(profile);
        printf("Profile name: %s\n", name);
        printf("Profile token: %s\n", token);

        gtk_list_store_insert_with_values(liststore, NULL, -1,
                                        // 0, "red",
                                        1, name,
                                        -1);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(evt->device->profile_dropdown),0);
    gtk_widget_set_sensitive (evt->device->profile_dropdown, TRUE);

exit:
    OnvifProfiles__destroy(evt->profiles);
    CObject__unref((CObject*)evt->device);
    free(evt);
    return FALSE;
}

//WIP Profile selection
void _priv_Device__load_profiles(void * user_data){
    Device * device = (Device *) user_data;
    printf("_priv_Device__load_profiles\n");

    if(!CObject__addref((CObject*)device)){
        printf("WARN _priv_Device__load_profiles - invalid object\n");
        return;
    }

    if(OnvifDevice__get_last_error(device->onvif_device) == ONVIF_NOT_AUTHORIZED){
        printf("WARN _priv_Device__load_profiles - unauthorized\n");
        goto exit;
    }

    OnvifMediaService * media_service = OnvifDevice__get_media_service(device->onvif_device);
    OnvifProfiles * profiles = OnvifMediaService__get_profiles(media_service);
    if(CObject__is_valid((CObject*)device) && OnvifDevice__get_last_error(device->onvif_device) == ONVIF_ERROR_NONE){
        GUIProfileEvent * evt = malloc(sizeof(GUIProfileEvent));
        evt->device = device;
        evt->profiles = profiles;
        gdk_threads_add_idle((void *)gui_Device__display_profiles,evt);
    } else {
        OnvifProfiles__destroy(profiles);
    }

exit:
    printf("_priv_Device__load_profiles - Done\n");
    CObject__unref((CObject*)device);
}

void Device__init(Device* self, OnvifDevice * onvif_device) {
    CObject__init((CObject*)self);
    CObject__set_destroy_callback((CObject*)self,priv_Device__destroy);
    self->onvif_device = onvif_device;
    self->selected=0;
    self->profile_index=0;
    self->profile_callback = NULL;
    self->profile_userdata = NULL;
    self->ref_lock =malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(self->ref_lock, NULL);
}

Device * Device__create(OnvifDevice * onvif_device){
    Device* result = (Device*) malloc(sizeof(Device));
    Device__init(result, onvif_device);
    return result;
}

void Device__lookup_hostname(Device* device, EventQueue * queue){
    printf("Device__lookup_hostname\n");
    EventQueue__insert(queue,_priv_Device__lookup_hostname,device);
}

void Device__load_thumbnail(Device* device, EventQueue * queue){
    printf("Device__load_thumbnail\n");
    EventQueue__insert(queue,_priv_Device__load_thumbnail,device);
}

GtkWidget * Device__create_row (Device * device, char * uri, char* name, char * hardware, char * location){
    GtkWidget *row;
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *image;

    row = gtk_list_box_row_new ();

    grid = gtk_grid_new ();
    g_object_set (grid, "margin", 5, NULL);

    image = gtk_spinner_new ();
    gtk_spinner_start (GTK_SPINNER (image));

    GtkWidget * thumbnail_handle = gtk_event_box_new ();
    device->image_handle = thumbnail_handle;
    gtk_container_add (GTK_CONTAINER (thumbnail_handle), image);
    g_object_set (thumbnail_handle, "margin-end", 10, NULL);
    gtk_grid_attach (GTK_GRID (grid), thumbnail_handle, 0, 1, 1, 3);

    char* markup_name = malloc(strlen("<b>") + strlen(name) + strlen("</b>") +1);
    strcpy(markup_name, "<b>");
    strcat(markup_name, name);
    strcat(markup_name, "</b>");
    label = gtk_label_new (NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup_name);
    g_object_set (label, "margin-end", 5, NULL);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

    char * dev_ip = OnvifDevice__get_ip(device->onvif_device);
    label = gtk_label_new (dev_ip);
    g_object_set (label, "margin-top", 5, "margin-end", 5, NULL);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);
    free(dev_ip);

    label = gtk_label_new (hardware);
    g_object_set (label, "margin-top", 5, "margin-end", 5, NULL);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

    label = gtk_label_new (location);
    gtk_label_set_ellipsize (GTK_LABEL(label),PANGO_ELLIPSIZE_END);
    g_object_set (label, "margin-top", 5, "margin-end", 5, NULL);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (grid), label, 1, 3, 1, 1);

    //Create profile dropdown placeholder
    GtkListStore *liststore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    device->profile_dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(liststore));
    g_object_unref(liststore);

    GtkCellRenderer  * column = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(device->profile_dropdown), column, TRUE);

    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(device->profile_dropdown), column,
                                    "cell-background", 0,
                                    "text", 1,
                                    NULL);
                                    
    gtk_widget_set_sensitive (device->profile_dropdown, FALSE);
    gtk_grid_attach (GTK_GRID (grid), device->profile_dropdown, 0, 4, 2, 1);

    g_signal_connect (G_OBJECT (device->profile_dropdown), "changed", G_CALLBACK (priv_Device__profile_changed), device);

    gtk_container_add (GTK_CONTAINER (row), grid);
  
    //For some reason, spinner has a floating ref
    //This is required to keep ability to remove the spinner later
    g_object_ref(image);

    return row;
}

void Device__set_profile_callback(Device * self, void (*profile_callback)(Device *, void *), void * profile_userdata){
    self->profile_callback = profile_callback;
    self->profile_userdata = profile_userdata;
}

void Device__load_profiles(Device* device, EventQueue * queue){
    printf("Device__load_profiles\n");
    EventQueue__insert(queue,_priv_Device__load_profiles,device);
}

OnvifDevice * Device__get_device(Device * self){
    return self->onvif_device;
}

int Device__is_selected(Device * self){
    return self->selected;
}

void Device__set_selected(Device * self, int selected){
    self->selected = selected;
}

int Device__get_selected_profile(Device * self){
    int ret = 0;
    pthread_mutex_lock(self->ref_lock);
    ret = self->profile_index;
    pthread_mutex_unlock(self->ref_lock);
    return ret;
}

void Device__set_thumbnail(Device * self, GtkWidget * image){
    gui_update_widget_image(image,self->image_handle);
}