#include <ekn-broadway-window.h>

int
main (int argc, char **argv)
{
  GtkWidget *window, *label;

  gtk_init (&argc, &argv);

  /* Create main window */
  window = ekn_broadway_window_new ();

  label = gtk_label_new ("A simple Label");
  gtk_container_add (GTK_CONTAINER (window), label);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
