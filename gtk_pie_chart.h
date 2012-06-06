/*
 * Gtk Pie Chart Widget
 * Copyright (C) 2008 Aron Rubin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 */
 
#ifndef GTK_PIE_CHART_H_INCLUDED
#define GTK_PIE_CHART_H_INCLUDED
 
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
 
G_BEGIN_DECLS
 
#define GTK_PIE_CHART(obj)	   GTK_CHECK_CAST( obj, gtk_pie_chart_get_type(), GtkPieChart )
#define GTK_PIE_CHART_CLASS(klass) GTK_CHECK_CLASS_CAST( klass, gtk_pie_chart_get_type(), GtkPieChartClass )
#define GTK_IS_PIE_CHART(obj)	   GTK_CHECK_TYPE( obj, gtk_pie_chart_get_type() )
 
typedef struct GtkPieChart GtkPieChart;
typedef struct GtkPieChartClass GtkPieChartClass;
 
typedef struct GtkPieSegment {
  float sweep;
  char *name;
  float rgba[4];
} GtkPieSegment;
 
struct GtkPieChart {
  GtkWidget base;
 
  int width;
  int height;
 
  float start_angle;
  float sweep_factor;
  float radius;
 
  GList *segments;
};
 
struct GtkPieChartClass {
  GtkWidgetClass parent_class;
};
 
GtkType gtk_pie_chart_get_type();
GtkWidget *gtk_pie_chart_new();
guint gtk_pie_chart_add_segment( GtkPieChart *pie, float sweep, const gchar *name, float *rgba );
void gtk_pie_chart_remove_segment( GtkPieChart *pie, guint id );
 
G_END_DECLS
 
#endif
