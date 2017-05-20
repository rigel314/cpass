/*
 * gui-gtk.c
 *
 *  Created on: Apr 22, 2017
 *      Author: cody
 */

#include "../main/gui.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "../main/sql.h"

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

struct catagoryButData
{
	int id;
	GObject* box;
};

struct itemButData
{
	int id;
	GObject* itemBox;
	GObject* dataGrid;
};

// Internal GUI functions
static void populateCatagories(GObject* butAllItems, GObject* boxCats);
static void populateTags(GObject* butAllItems, GObject* boxTags);
static void hilightBut(GtkWidget* but, gpointer data);
static void deleteBut(GtkWidget* but, gpointer data);
GObject* g_object_clone(GObject *src);

// Callbacks
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
static void destroy(GtkWidget* widget, gpointer data)
{
	gtk_main_quit();
}
static void catagoryBut_Click(GtkButton* but, gpointer data)
{
	struct catagoryButData* x = data;
	
	gtk_container_foreach(GTK_CONTAINER(x->box), hilightBut, but);
	
	printf("id: %d\n", x->id);
}
static void catagoryBut_Destroy(GtkButton* but, gpointer data)
{
	free(data);
}
static void copyBut_Click(GtkButton* but, gpointer data)
{
	GtkClipboard* cb = gtk_clipboard_get(gdk_atom_intern("CLIPBOARD", TRUE));
	if(!cb)
		return;
	
	gtk_clipboard_set_text(cb, gtk_entry_get_text((GtkEntry*)data), -1);
}
static void copyBut_Destroy(GtkButton* but, gpointer data)
{
//	free(data);
}
static void showBut_Click(GtkButton* but, gpointer data)
{
	GtkWidget* txt = (GtkWidget*)data;
	
	GValue curName = G_VALUE_INIT;
	g_value_init(&curName, G_TYPE_STRING);
	g_object_get_property((GObject*)but, "label", &curName);
	
	int show = !strcmp("Show", g_value_get_string(&curName));
	
	GValue visibility = G_VALUE_INIT;
	g_value_init(&visibility, G_TYPE_BOOLEAN);
	g_value_set_boolean(&visibility, (show)?TRUE:FALSE);
	g_object_set_property((GObject*)txt, "visibility", &visibility);
	
	GValue name = G_VALUE_INIT;
	g_value_init(&name, G_TYPE_STRING);
	g_value_set_string(&name, (show)?"Hide":"Show");
	g_object_set_property((GObject*)but, "label", &name);
}
static void showBut_Destroy(GtkButton* but, gpointer data)
{
//	free(data);
}
static void itemBut_Click(GtkButton* button, gpointer data)
{
	struct itemButData* x = data;
	
	gtk_container_foreach(GTK_CONTAINER(x->dataGrid), deleteBut, NULL);
	
	GtkWidget* title = gtk_label_new(gtk_button_get_label(button));
	gtk_widget_show(title);
	gtk_grid_attach((GtkGrid*)x->dataGrid, title, 0, 0, 4, 1);
	
	char* ptJSON;
	getItemByID(&ptJSON, x->id);
	
//	dbgLog("%s\n", ptJSON);
	
	struct json_object* jobj = json_tokener_parse(ptJSON);
	struct json_object* val;

	int rowNum = 1;
	
	// Process main fields
	if(json_object_object_get_ex(jobj, "fields", &val) && json_object_is_type(val, json_type_array))
	{
		int len = json_object_array_length(val);
		for(int i = 0; i < len; i++)
		{
			struct json_object* arrVal = json_object_array_get_idx(val, i);
			struct json_object* des;
			struct json_object* desVal;
			struct json_object* desType;
			if(json_object_object_get_ex(arrVal, "designation", &des) && json_object_object_get_ex(arrVal, "value", &desVal))
			{
				GtkWidget* lbl = gtk_label_new(json_object_get_string(des));
				
				GtkWidget* txt = gtk_entry_new();
				gtk_entry_set_text((GtkEntry*)txt, json_object_get_string(desVal));
				GValue gvfalse = G_VALUE_INIT;
				g_value_init(&gvfalse, G_TYPE_BOOLEAN);
				g_value_set_boolean(&gvfalse, FALSE);
				g_object_set_property((GObject*)txt, "editable", &gvfalse);
				GValue width = G_VALUE_INIT;
				g_value_init(&width, G_TYPE_INT);
				g_value_set_int(&width, 30);
				g_object_set_property((GObject*)txt, "width-chars", &width);
				if(json_object_object_get_ex(arrVal, "type", &desType) && !strcmp("P", json_object_get_string(desType)))
				{
					g_object_set_property((GObject*)txt, "visibility", &gvfalse);
					GtkWidget* butsh = gtk_button_new_with_label("Show");
					g_signal_connect(butsh, "clicked", G_CALLBACK(showBut_Click), txt);
					g_signal_connect(butsh, "destroy", G_CALLBACK(showBut_Destroy), NULL);
					gtk_widget_show(butsh);
					gtk_grid_attach((GtkGrid*)x->dataGrid, butsh, 3, rowNum, 1, 1);
				}
				
				GtkWidget* butcp = gtk_button_new_with_label("Copy");
				g_signal_connect(butcp, "clicked", G_CALLBACK(copyBut_Click), txt);
				g_signal_connect(butcp, "destroy", G_CALLBACK(copyBut_Destroy), NULL);
				
				gtk_widget_show(lbl);
				gtk_widget_show(txt);
				gtk_widget_show(butcp);
				
				gtk_grid_attach((GtkGrid*)x->dataGrid, lbl, 0, rowNum, 1, 1);
				gtk_grid_attach((GtkGrid*)x->dataGrid, txt, 1, rowNum, 1, 1);
				gtk_grid_attach((GtkGrid*)x->dataGrid, butcp, 2, rowNum, 1, 1);
				
				rowNum++;
			}
		}
	}
	
	// Process sections
	if(json_object_object_get_ex(jobj, "sections", &val) && json_object_is_type(val, json_type_array))
	{
		int len = json_object_array_length(val);
		for(int i = 0; i < len; i++)
		{
			struct json_object* secVal = json_object_array_get_idx(val, i);
			struct json_object* secName;
			if(json_object_object_get_ex(secVal, "title", &secName))
			{
				GtkWidget* hr = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
				
				GtkWidget* lbl = gtk_label_new(json_object_get_string(secName));
				
				gtk_widget_show(hr);
				gtk_widget_show(lbl);
				
				gtk_grid_attach((GtkGrid*)x->dataGrid, hr, 0, rowNum, 4, 1);
				gtk_grid_attach((GtkGrid*)x->dataGrid, lbl, 0, rowNum+1, 1, 1);
				rowNum++;
				rowNum++;
			}
			
			struct json_object* secFields;
			if(json_object_object_get_ex(secVal, "fields", &secFields) && json_object_is_type(secFields, json_type_array))
			{
				int len = json_object_array_length(secFields);
				for(int j = 0; j < len; j++)
				{
					struct json_object* arrVal = json_object_array_get_idx(secFields, j);
					struct json_object* des;
					struct json_object* desVal;
					struct json_object* desType;
					if(json_object_object_get_ex(arrVal, "t", &des) && json_object_object_get_ex(arrVal, "v", &desVal) && json_object_object_get_ex(arrVal, "k", &desType))
					{
						GtkWidget* lbl = gtk_label_new(json_object_get_string(des));
						
						GtkWidget* txt = gtk_entry_new();
						gtk_entry_set_text((GtkEntry*)txt, json_object_get_string(desVal));
						GValue gvfalse = G_VALUE_INIT;
						g_value_init(&gvfalse, G_TYPE_BOOLEAN);
						g_value_set_boolean(&gvfalse, FALSE);
						g_object_set_property((GObject*)txt, "editable", &gvfalse);
						GValue width = G_VALUE_INIT;
						g_value_init(&width, G_TYPE_INT);
						g_value_set_int(&width, 30);
						g_object_set_property((GObject*)txt, "width-chars", &width);
						if(!strcmp("concealed", json_object_get_string(desType)))
						{
							g_object_set_property((GObject*)txt, "visibility", &gvfalse);
							GtkWidget* butsh = gtk_button_new_with_label("Show");
							g_signal_connect(butsh, "clicked", G_CALLBACK(showBut_Click), txt);
							g_signal_connect(butsh, "destroy", G_CALLBACK(showBut_Destroy), NULL);
							gtk_widget_show(butsh);
							gtk_grid_attach((GtkGrid*)x->dataGrid, butsh, 3, rowNum, 1, 1);
						}
						
						GtkWidget* butcp = gtk_button_new_with_label("Copy");
						g_signal_connect(butcp, "clicked", G_CALLBACK(copyBut_Click), txt);
						g_signal_connect(butcp, "destroy", G_CALLBACK(copyBut_Destroy), NULL);
						
						gtk_widget_show(lbl);
						gtk_widget_show(txt);
						gtk_widget_show(butcp);
						
						gtk_grid_attach((GtkGrid*)x->dataGrid, lbl, 0, rowNum, 1, 1);
						gtk_grid_attach((GtkGrid*)x->dataGrid, txt, 1, rowNum, 1, 1);
						gtk_grid_attach((GtkGrid*)x->dataGrid, butcp, 2, rowNum, 1, 1);
						
						rowNum++;
					}
				}
			}
		}
	}
	
	// Process password-only items
	if(json_object_object_get_ex(jobj, "password", &val))
	{
		GtkWidget* lbl = gtk_label_new("Password");

		GtkWidget* txt = gtk_entry_new();
		gtk_entry_set_text((GtkEntry*)txt, json_object_get_string(val));
		GValue gvfalse = G_VALUE_INIT;
		g_value_init(&gvfalse, G_TYPE_BOOLEAN);
		g_value_set_boolean(&gvfalse, FALSE);
		g_object_set_property((GObject*)txt, "editable", &gvfalse);
		g_object_set_property((GObject*)txt, "visibility", &gvfalse);
		GValue width = G_VALUE_INIT;
		g_value_init(&width, G_TYPE_INT);
		g_value_set_int(&width, 30);
		g_object_set_property((GObject*)txt, "width-chars", &width);

		GtkWidget* butcp = gtk_button_new_with_label("Copy");
		g_signal_connect(butcp, "clicked", G_CALLBACK(copyBut_Click), txt);
		g_signal_connect(butcp, "destroy", G_CALLBACK(copyBut_Destroy), NULL);
		
		GtkWidget* butsh = gtk_button_new_with_label("Show");
		g_signal_connect(butsh, "clicked", G_CALLBACK(showBut_Click), txt);
		g_signal_connect(butsh, "destroy", G_CALLBACK(showBut_Destroy), NULL);

		gtk_widget_show(txt);
		gtk_widget_show(lbl);
		gtk_widget_show(butcp);
		gtk_widget_show(butsh);

		gtk_grid_attach((GtkGrid*)x->dataGrid, lbl, 0, rowNum, 1, 1);
		gtk_grid_attach((GtkGrid*)x->dataGrid, txt, 1, rowNum, 1, 1);
		gtk_grid_attach((GtkGrid*)x->dataGrid, butcp, 2, rowNum, 1, 1);
		gtk_grid_attach((GtkGrid*)x->dataGrid, butsh, 3, rowNum, 1, 1);
		
		rowNum++;
	}

	// Process notes
	if(json_object_object_get_ex(jobj, "notesPlain", &val) && json_object_get_string_len(val) > 0)
	{
		GtkWidget* lbl = gtk_label_new("Secure note:");

		GtkWidget* txt = gtk_text_view_new();
		GtkTextBuffer* buf = gtk_text_view_get_buffer((GtkTextView*)txt);
		gtk_text_buffer_set_text(buf, json_object_get_string(val), -1);
		GValue gvfalse = G_VALUE_INIT;
		g_value_init(&gvfalse, G_TYPE_BOOLEAN);
		g_value_set_boolean(&gvfalse, FALSE);
		g_object_set_property((GObject*)txt, "editable", &gvfalse);

		gtk_widget_show(txt);
		gtk_widget_show(lbl);

		gtk_grid_attach((GtkGrid*)x->dataGrid, lbl, 0, rowNum, 1, 1);
		gtk_grid_attach((GtkGrid*)x->dataGrid, txt, 1, rowNum+1, 3, 1);
		
		rowNum++;
		rowNum++;
	}

	json_object_put(jobj);
}
static void itemBut_Destroy(GtkButton* but, gpointer data)
{
	free(data);
}
// End Callbacks

// Internal GUI functions
static void populateCatagories(GObject* butAllItems, GObject* boxCats)
{
	// destroy all existing children in boxCats
	gtk_container_foreach(GTK_CONTAINER(boxCats), deleteBut, NULL);
	
	// create children in boxCats for each tag
	struct catagory* cats = NULL;
	int numCats = getCatagories(&cats);
	for(int i = 0; i < numCats; i++)
	{
		GObject* but = g_object_clone(butAllItems);
		if(!but)
			break;
		
		gtk_button_set_label((GtkButton*)but, cats[i].name);
		struct catagoryButData* data = malloc(sizeof(struct catagoryButData));
		if(!data)
		{
			gtk_widget_destroy((GtkWidget*)but);
			break;
		}
		data->id = cats[i].id;
		data->box = boxCats;
		g_signal_connect(but, "clicked", G_CALLBACK(catagoryBut_Click), data);
		g_signal_connect(but, "destroy", G_CALLBACK(catagoryBut_Destroy), data);
		gtk_container_add(GTK_CONTAINER(boxCats), (GtkWidget*)but);
		gtk_widget_show((GtkWidget*)but);
		
		free(cats[i].name);
	}
	free(cats);
}

static void populateTags(GObject* butAllItems, GObject* boxTags)
{
	// destroy all existing children in boxTags
	gtk_container_foreach(GTK_CONTAINER(boxTags), deleteBut, NULL);
	
	// create children in boxTags for each tag
	char** tags;
	int numTags = getTags(&tags);
	
	for(int i = 0; i < numTags; i++)
	{
		GObject* but = g_object_clone(butAllItems);
		if(!but)
			break;
		
		gtk_button_set_label((GtkButton*)but, tags[i]);
		
		gtk_container_add(GTK_CONTAINER(boxTags), (GtkWidget*)but);
		gtk_widget_show((GtkWidget*)but);

		free(tags[i]);
	}
	free(tags);
}

static void populateTitles(GObject* butAllItems, GObject* boxItems, GObject* dataGrid)
{
	// destroy all existing children in boxItems
	gtk_container_foreach(GTK_CONTAINER(boxItems), deleteBut, NULL);

	// create children in boxItems for each item
	struct item* items;
	int numItems = getItems(&items);
	
	for(int i = 0; i < numItems; i++)
	{
		GObject* but = g_object_clone(butAllItems);
		if(!but)
			break;
		
		gtk_button_set_label((GtkButton*)but, items[i].name);
		
		struct itemButData* data = malloc(sizeof(struct itemButData));
		if(!data)
		{
			gtk_widget_destroy((GtkWidget*)but);
			break;
		}
		data->id = items[i].id;
		data->itemBox = boxItems;
		data->dataGrid = dataGrid;
		g_signal_connect(but, "clicked", G_CALLBACK(itemBut_Click), data);
		g_signal_connect(but, "destroy", G_CALLBACK(itemBut_Destroy), data);
		
		gtk_container_add(GTK_CONTAINER(boxItems), (GtkWidget*)but);
		gtk_widget_show((GtkWidget*)but);

		free(items[i].name);
	}
	free(items);
}

static void hilightBut(GtkWidget* but, gpointer data)
{
	GValue relief = G_VALUE_INIT;
	g_value_init(&relief, GTK_TYPE_RELIEF_STYLE);
	if(but == data)
		g_value_set_enum(&relief, GTK_RELIEF_NORMAL);
	else
		g_value_set_enum(&relief, GTK_RELIEF_NONE);
	g_object_set_property((GObject*)but, "relief", &relief);
}

static void deleteBut(GtkWidget* but, gpointer data)
{
	gtk_widget_destroy(but);
}

// Thank you stack overflow: http://stackoverflow.com/questions/3003655/is-there-a-good-way-to-copy-a-gtk-widget
GObject* g_object_clone(GObject *src)
{
	GObject *dst;
	GParameter *params;
	GParamSpec **specs;
	guint n, n_specs, n_params;

	specs = g_object_class_list_properties(G_OBJECT_GET_CLASS(src), &n_specs);
	params = g_new0(GParameter, n_specs);
	if(!params)
	{
		g_free(specs);
		return NULL;
	}
	n_params = 0;

	for(n = 0; n < n_specs; n++)
		if(strcmp(specs[n]->name, "parent") && (specs[n]->flags & G_PARAM_READWRITE) == G_PARAM_READWRITE)
		{
			params[n_params].name = g_intern_string(specs[n]->name);
			g_value_init(&params[n_params].value, specs[n]->value_type);
			g_object_get_property(src, specs[n]->name, &params[n_params].value);
			n_params++;
		}

	dst = g_object_newv(G_TYPE_FROM_INSTANCE(src), n_params, params);
	g_free(specs);
	g_free(params);

	return dst;
}
// End Internal GUI functions 

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

ADDGTKPREFIX(int, showMainWin())
{
	GtkBuilder* builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, mainXML, -1, NULL);

	GObject* window = gtk_builder_get_object(builder, "window");
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);

	GObject* boxCats = gtk_builder_get_object(builder, "boxCatagoryList");
	GObject* boxTags = gtk_builder_get_object(builder, "boxTagList");
	GObject* boxItems = gtk_builder_get_object(builder, "boxItemList");
	GObject* gridData = gtk_builder_get_object(builder, "gridData");
	
	GObject* butAllItems = gtk_builder_get_object(builder, "butAllItems");
	
	populateCatagories(butAllItems, boxCats);
	
	populateTags(butAllItems, boxTags);
	
	populateTitles(butAllItems, boxItems, gridData);
	
	gtk_main();
	return 0;
}
