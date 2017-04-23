/*
 * gui-gtk.c
 *
 *  Created on: Apr 22, 2017
 *      Author: cody
 */

#include "../gui.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

struct gtkAskPassData
{
	GtkWidget* pass;
	char** out;
};

static gboolean delete_event(GtkWidget* widget, GdkEvent* event, gpointer data)
{
	return FALSE;
}
static void destroy(GtkWidget* widget, gpointer data)
{
	struct gtkAskPassData* gapd = (struct gtkAskPassData*)data;
	GValue txt = G_VALUE_INIT;
	
	g_value_init(&txt, G_TYPE_STRING);
	g_object_get_property((GObject*)gapd->pass, "text", &txt);
	const char* entryText = g_value_get_string(&txt);
	*(gapd->out) = malloc(strlen(entryText)+1);
	if(*(gapd->out))
		strcpy(*(gapd->out), entryText);
	g_value_unset(&txt);
	gtk_main_quit();
}
static void submitBut(GtkButton* button, gpointer data)
{
	gtk_widget_destroy(data);
}
static void passkeyup(GtkWidget* widget, GdkEvent* event, gpointer data)
{
	if(event->key.keyval == GDK_KEY_Return)
	{
		submitBut(NULL, data);
	}
}

ADDGTKPREFIX(int, initializeGUI())
{
	return 0;
}

ADDGTKPREFIX(int, askPass(const char* msg, char** out))
{
	GtkWidget* window;
	GtkWidget* pass;
	GtkWidget* submit;
	GtkWidget* vbox;
	
	// Initialize gtk
	gtk_init(0, NULL);

	// Setup the window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	// Setup a container box
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	
	// Setup the text entry
	GValue pass_placeHolder = G_VALUE_INIT;
	GValue pass_caps = G_VALUE_INIT;
	GValue pass_inputPurpose = G_VALUE_INIT;
	GValue pass_vis = G_VALUE_INIT;
	pass = gtk_entry_new();
	g_value_init(&pass_placeHolder, G_TYPE_STRING);
	g_value_set_static_string(&pass_placeHolder, msg);
	g_value_init(&pass_caps, G_TYPE_BOOLEAN);
	g_value_set_boolean(&pass_caps, TRUE);
	g_value_init(&pass_inputPurpose, G_TYPE_INT);
	g_value_set_int(&pass_inputPurpose, GTK_INPUT_PURPOSE_PASSWORD);
	g_value_init(&pass_vis, G_TYPE_BOOLEAN);
	g_value_set_boolean(&pass_vis, FALSE);
	g_object_set_property((GObject*)pass, "placeholder-text", &pass_placeHolder);
	g_object_set_property((GObject*)pass, "caps-lock-warning", &pass_caps);
	g_object_set_property((GObject*)pass, "input-purpose", &pass_inputPurpose);
	g_object_set_property((GObject*)pass, "visibility", &pass_vis);
	g_signal_connect(pass, "key-release-event", G_CALLBACK(passkeyup), window);

	// Setup the submit button
	submit = gtk_button_new_with_label ("Submit");
	g_signal_connect(submit, "clicked", G_CALLBACK(submitBut), window);

	// Setup window event handlers
	struct gtkAskPassData gapd = {.pass = pass, .out = out};
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), &gapd);
	
	// Build the display, and show everything
	gtk_container_add (GTK_CONTAINER(window), vbox);
	gtk_box_pack_start(GTK_BOX(vbox), pass, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), submit, FALSE, FALSE, 0);
	gtk_widget_show(pass);
	gtk_widget_show(submit);
	gtk_widget_show(vbox);
	gtk_widget_show(window);
	
	gtk_main();
	
	g_value_unset(&pass_placeHolder);
	g_value_unset(&pass_caps);
	g_value_unset(&pass_inputPurpose);
	g_value_unset(&pass_vis);
	
	return 0;
}
