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
#include "../sql.h"

static char* askpassXML = ""
#include "xml/gtkaskpass.pxml"
;
static char* mainXML = ""
#include "xml/gtkmain.pxml"
;

#ifdef DEBUG
	extern char pass[100];
#endif

struct gtkAskPassData
{
	GObject* pass;
	char** out;
};

static gboolean delete_event(GtkWidget* widget, GdkEvent* event, gpointer data)
{
	return FALSE;
}
static void ap_destroy(GtkWidget* widget, gpointer data)
{
	struct gtkAskPassData* gapd = (struct gtkAskPassData*)data;
	GValue txt = G_VALUE_INIT;
	
	g_value_init(&txt, G_TYPE_STRING);
	g_object_get_property(gapd->pass, "text", &txt);
	const char* entryText = g_value_get_string(&txt);
//	hexdump("ap\n", entryText, strlen(entryText)+1);
	if(gapd->out)
	{
		*(gapd->out) = malloc(strlen(entryText)+1);
		if(*(gapd->out))
			strcpy(*(gapd->out), entryText);
		#ifdef DEBUG
			if(strlen(entryText) == 0)
			{
				free(*(gapd->out));
				*(gapd->out) = malloc(100);
				strcpy(*(gapd->out), pass);
			}
		#endif
	}
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
	gtk_init(0, NULL);
	return 0;
}

ADDGTKPREFIX(int, askPass(const char* msg, char** out))
{
	GtkBuilder* builder;
	GObject* window;
	GObject* lblPrompt;
	GObject* pass;
	GObject* submit;
	
	// Build from gtkaskpass.xml
	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, askpassXML, -1, NULL);
	
	// Setup window
	window = gtk_builder_get_object(builder, "window");
	GValue window_title = G_VALUE_INIT;
	g_value_init(&window_title, G_TYPE_STRING);
	g_value_set_static_string(&window_title, msg);
	g_object_set_property(window, "title", &window_title);
	
	lblPrompt = gtk_builder_get_object(builder, "lblPrompt");
	g_object_set_property(lblPrompt, "label", &window_title);
	
	// Setup the text entry
	pass = gtk_builder_get_object(builder, "pass");
	GValue pass_placeHolder = G_VALUE_INIT;
	g_value_init(&pass_placeHolder, G_TYPE_STRING);
	g_value_set_static_string(&pass_placeHolder, msg);
	g_object_set_property(pass, "placeholder-text", &pass_placeHolder);
	g_signal_connect(pass, "key-release-event", G_CALLBACK(passkeyup), window);
	
	// Setup the submit button
	submit = gtk_builder_get_object(builder, "submit");
	g_signal_connect(submit, "clicked", G_CALLBACK(submitBut), window);
	
	// Setup window event handlers
	struct gtkAskPassData gapd = {.pass = pass, .out = out};
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(ap_destroy), &gapd);
	
	gtk_main();
	
	return 0;
}

static void destroy(GtkWidget* widget, gpointer data)
{
	gtk_main_quit();
}

GObject* g_object_clone(GObject *src)
{
	GObject *dst;
	GParameter *params;
	GParamSpec **specs;
	guint n, n_specs, n_params;

	specs = g_object_class_list_properties(G_OBJECT_GET_CLASS(src), &n_specs);
	params = g_new0(GParameter, n_specs);
	n_params = 0;

	for(n = 0; n < n_specs; n++)
		if(strcmp(specs[n]->name, "parent") && (specs[n]->flags & G_PARAM_READWRITE) == G_PARAM_READWRITE)
		{
			params[n_params].name = g_intern_string(specs[n]->name);
			g_value_init(&params[n_params].value, specs[n]->value_type);
			g_object_get_property(src, specs[n]->name, &params[n_params].value);
			++ n_params;
		}

	dst = g_object_newv(G_TYPE_FROM_INSTANCE(src), n_params, params);
	g_free(specs);
	g_free(params);

	return dst;
}

ADDGTKPREFIX(int, showMainWin())
{
	GtkBuilder* builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, mainXML, -1, NULL);

	GObject* window = gtk_builder_get_object(builder, "window");
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);

	GObject* boxCats = gtk_builder_get_object(builder, "boxCatagoryList");
	GObject* boxTags = gtk_builder_get_object(builder, "boxTagList");
	
	GObject* butAllItems = gtk_builder_get_object(builder, "butAllItems");
	
	char** cats = NULL;
	int numCats = getCatagories(&cats);
	for(int i = 0; i < numCats; i++)
	{
		GObject* but = g_object_clone(butAllItems);
		gtk_button_set_label((GtkButton*)but, cats[i]);
		gtk_container_add(GTK_CONTAINER(boxCats), (GtkWidget*)but);
		gtk_widget_show((GtkWidget*)but);
		free(cats[i]);
	}
	free(cats);
	
	(void) boxTags;
	
	printItemJSON();
	
	gtk_main();
	return 0;
}
