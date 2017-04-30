#include <gtk/gtk.h>

static gboolean delete_event(GtkWidget* widget, GdkEvent* event, gpointer data)
{
	return FALSE;
}
static void destroy(GtkWidget* widget, gpointer data)
{
	gtk_main_quit();
}

int main()
{
	printf("init\n");
	gtk_init(0, NULL);

	printf("load xml file\n");
	GtkBuilder* b = gtk_builder_new_from_file("./src/gui/xml/aoeu.glade");
	
	if(!b)
		return 1;
	
	printf("loaded, connecting signals\n");
	GObject* window = gtk_builder_get_object(b, "window");
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
	
	printf("show main\n");
	gtk_main();
}
