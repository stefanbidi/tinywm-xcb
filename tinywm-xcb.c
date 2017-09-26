/* TinyWM is written by Nick Welch <nick@incise.org> in 2005 & 2011.
 * TinyWM_xcb is a port of Nick Welch's TinyWM to the XCB library written
 * in 2015.
 *
 * This software is in the public domain
 * and is provided AS IS, with NO WARRANTY. */

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_aux.h>
#include <stdlib.h>
#include <stdio.h>

xcb_connection_t *connection;
xcb_screen_t *screen;
xcb_window_t rootWindow;

void wmInitialize (void)
{
  int defaultScreen;

  /* Connect to the X server */
  connection = xcb_connect (NULL, &defaultScreen);
  if (xcb_connection_has_error (connection) > 0) {
    fprintf (stderr, "Could not open connection to X Server!\n");
    exit (EXIT_FAILURE);
  }

  printf ("Default screen # = %d\n", defaultScreen);
  screen = xcb_aux_get_screen (connection, defaultScreen);;
  rootWindow = screen->root;
  printf ("Screen size = %dx%d\n", screen->width_in_pixels, screen->height_in_pixels);
  printf ("rootWindow = %d\n", rootWindow);

  xcb_grab_server (connection);

  /* This causes an error in another window manager is running */
  {
    xcb_void_cookie_t cookie;
    const uint32_t value = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;

    cookie = xcb_change_window_attributes_checked (connection, rootWindow, XCB_CW_EVENT_MASK, &value);
    if (xcb_request_check (connection, cookie)) {
      fprintf (stderr, "It seems that another window manager is already running!\n");
      xcb_disconnect (connection);
      exit (EXIT_FAILURE);
    }
  }

  xcb_ungrab_server (connection);

  /* Mouse button bindings */
  xcb_grab_button (connection, 0, rootWindow, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
                   XCB_GRAB_MODE_ASYNC, rootWindow, XCB_NONE, 1, XCB_MOD_MASK_1);
  xcb_grab_button (connection, 0, rootWindow, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
                   XCB_GRAB_MODE_ASYNC, rootWindow, XCB_NONE, 3, XCB_MOD_MASK_1);
  xcb_grab_button (connection, 0, rootWindow, XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, rootWindow,
                   XCB_NONE, 1, XCB_NONE);

  xcb_flush (connection);
}

int main (void)
{
  xcb_window_t focused_window;
  xcb_window_t window;
  xcb_generic_event_t *event;
  uint32_t attr[5];

  xcb_get_geometry_reply_t *geometry;

  wmInitialize ();

  focused_window = rootWindow;
  while (1) {
    event = xcb_wait_for_event (connection);

    switch (event->response_type & ~0x80) {
    case XCB_BUTTON_PRESS:
      {
        xcb_button_press_event_t *e;

        e = (xcb_button_press_event_t *) event;
        printf ("Button press = %d, Modifier = %d\n", e->detail, e->state);
        window = e->child;
        attr[0] = XCB_STACK_MODE_ABOVE;
        xcb_configure_window (connection, window, XCB_CONFIG_WINDOW_STACK_MODE, attr);
        geometry = xcb_get_geometry_reply (connection, xcb_get_geometry (connection, window), NULL);
        printf ("e->response_type = %d, e->sequence = %d, e->detail = %d, e->state = %d\n", e->response_type, e->sequence,
                e->detail, e->state);
        if (e->detail == 1) {
          if (e->state == XCB_MOD_MASK_1) {
            attr[4] = 1;
            xcb_warp_pointer (connection, XCB_NONE, window, 0, 0, 0, 0, 1, 1);
            xcb_grab_pointer (connection, 0, rootWindow,
                              XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT,
                              XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, rootWindow, XCB_NONE, XCB_CURRENT_TIME);
          } else {
            if (focused_window != window) {
            printf ("focusing window %d\n", window);
            focused_window = window;
/*            xcb_ungrab_button (connection, 1, rootWindow, XCB_NONE);
            xcb_grab_button (connection, 0, window, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, rootWindow, XCB_NONE, XCB_BUTTON_INDEX_ANY, XCB_MOD_MASK_ANY);
*/            xcb_set_input_focus (connection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
            }
          }
        } else {
          attr[4] = 3;
          xcb_warp_pointer (connection, XCB_NONE, window, 0, 0, 0, 0, geometry->width, geometry->height);
          xcb_grab_pointer (connection, 0, rootWindow,
                            XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT,
                            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, rootWindow, XCB_NONE, XCB_CURRENT_TIME);
        }
        xcb_flush (connection);
      }
      break;

    case XCB_MOTION_NOTIFY:
      {
        xcb_query_pointer_reply_t *pointer;

        pointer = xcb_query_pointer_reply (connection, xcb_query_pointer (connection, rootWindow), 0);
        geometry = xcb_get_geometry_reply (connection, xcb_get_geometry (connection, window), NULL);
        if (attr[4] == 1) {     /* move */
          if (pointer->root_x > screen->width_in_pixels - 5) {
            attr[0] = screen->width_in_pixels / 2;
            attr[1] = 0;
            attr[2] = screen->width_in_pixels / 2;
            attr[3] = screen->height_in_pixels;
            xcb_configure_window (connection, window,
                                  XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                                  XCB_CONFIG_WINDOW_HEIGHT, attr);
          } else {
            attr[0] = (pointer->root_x + geometry->width > screen->width_in_pixels)
              ? (screen->width_in_pixels - geometry->width) : pointer->root_x;
            attr[1] = (pointer->root_y + geometry->height > screen->height_in_pixels)
              ? (screen->height_in_pixels - geometry->height) : pointer->root_y;
            xcb_configure_window (connection, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, attr);
          }
          xcb_flush (connection);
        } else if (attr[4] == 3) {      /* resize */
          attr[0] = pointer->root_x - geometry->x;
          attr[1] = pointer->root_y - geometry->y;
          xcb_configure_window (connection, window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, attr);
          xcb_flush (connection);
        }
      }
      break;

    case XCB_BUTTON_RELEASE:
      xcb_ungrab_pointer (connection, XCB_CURRENT_TIME);
      xcb_flush (connection);
    }

    free (event);
  }

  xcb_disconnect (connection);
}
