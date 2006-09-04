#include "e.h"

//#define DEBUG 1


struct order_data
{
   char order_path[PATH_MAX];
   Ecore_Sheap *sheap;
};

static int _e_menu_make_apps(const void *data, Ecore_Desktop_Tree * tree, int element, int level);
static void _e_menu_dump_each_hash_node(void *value, void *user_data);
static void _e_fdo_menu_to_order_add_sheap(Ecore_Sheap *sheap, const char *order_path, const char *file);

EAPI void
e_fdo_menu_to_order(void)
{
   int i;
   char *menu_file;
   
   /* Find the main menu file. */
   menu_file = ecore_desktop_paths_file_find(ecore_desktop_paths_menus, 
						  "applications.menu", -1, NULL, NULL);
   if (!menu_file)
      {
         /* Try various quirks of various systems. */
         menu_file = ecore_desktop_paths_file_find(ecore_desktop_paths_menus, 
						  "debian-menu.menu", -1, NULL, NULL);
         /* FIXME: If all else fails, run debians funky menu generator shit. */
      }

   if (menu_file)
      {
         char *path;
	     
	 path = ecore_file_get_dir(menu_file);
	 if (path)
	    {
	       Ecore_Desktop_Tree *menus;
		  
	       /* convert the xml into menus */
	       menus = ecore_desktop_menu_get(menu_file);
	       if (menus)
	          {
                     char dir[PATH_MAX];

                     /* Nuke the old menus. */
                     snprintf(dir, sizeof(dir), "%s/.e/e/applications/menu/all/", ecore_desktop_home_get());
                     ecore_file_recursive_rm(dir);
		     /* create the .desktop and order files from the menu */
		     ecore_desktop_tree_foreach(menus, 0, _e_menu_make_apps, path);
// FIXME: Can't free this just yet, causes major memory corruption.
//		     ecore_desktop_tree_del(menus);
		  }
	       free(path);
	       
	    }
	 free(menu_file);
      }
}

static int
_e_menu_make_apps(const void *data, Ecore_Desktop_Tree * tree, int element, int level)
{
   if (tree->elements[element].type == ECORE_DESKTOP_TREE_ELEMENT_TYPE_STRING)
     {
        if (strncmp((char *)tree->elements[element].element, "<MENU ", 6) == 0)
          {
	     struct order_data order_data;
             char *path;
             char *name;
             Ecore_Hash *apps;

             path = (char *)tree->elements[element + 1].element;
#ifdef DEBUG
             name = (char *)tree->elements[element].element;
             printf("MAKING MENU - %s \t\t%s\n", path, name);
#endif
//             pool = (Ecore_Hash *) tree->elements[element + 2].element;
             apps = (Ecore_Hash *) tree->elements[element + 4].element;

             order_data.sheap = ecore_sheap_new(ecore_str_compare, 100);
	     if (order_data.sheap)
	        {
                   ecore_sheap_set_free_cb(order_data.sheap, free);
                   snprintf(order_data.order_path, sizeof(order_data.order_path), "%s/.e/e/applications/menu/all/%s", ecore_desktop_home_get(), &path[11]);
                   /* Check if we need to create the directory. */
                   if (!ecore_file_exists(order_data.order_path))
                      {
                         const char *temp;

                         ecore_file_mkpath(order_data.order_path);
                         /* If we create a dir, we add it to the parents .order file. */
                         temp = ecore_file_get_dir((const char *) order_data.order_path);
	                 if (temp)
	                    {
                               _e_fdo_menu_to_order_add_sheap(order_data.sheap, temp, ecore_file_get_file(order_data.order_path));
		               free((char *) temp);
			       /* We just used the sheap, but we want it clear for next time. */
                               ecore_sheap_destroy(order_data.sheap);
                               order_data.sheap = ecore_sheap_new(ecore_str_compare, 100);
                               ecore_sheap_set_free_cb(order_data.sheap, free);
		            }
	                 temp = ecore_file_get_file(order_data.order_path);
	                 if (temp)
	                    {
// FIXME: What to do about .directory.eap's when .desktop takes over?
//                       create_dir_eap(order_path, temp);
		            }
                      }
                   /* Create the apps. */
                   ecore_hash_for_each_node(apps, _e_menu_dump_each_hash_node, &order_data);
                   _e_fdo_menu_to_order_add_sheap(order_data.sheap, order_data.order_path, NULL);
                   ecore_sheap_destroy(order_data.sheap);
		}
          }
     }
   return 0;
}

static void
_e_menu_dump_each_hash_node(void *value, void *user_data)
{
   struct order_data *order_data;
   Ecore_Hash_Node *node;
   Ecore_Desktop *desktop;
   const char *file, *key;

   order_data = (struct order_data *)user_data;
   node = (Ecore_Hash_Node *) value;
   key = (char *)node->key;
   file = (char *)node->value;
   desktop = ecore_desktop_get(file, NULL);
   /* Check if we process */
   if ( (!desktop->hidden) && (!desktop->no_display) && ((desktop->type == NULL) || (strcmp(desktop->type, "Application") == 0)) )
      {
         char path2[PATH_MAX];

#ifdef DEBUG
         printf("MAKING MENU ITEM %s -> %s  (%s)\n", order_data->order_path, file, key);
#endif
         snprintf(path2, sizeof(path2), "%s/.e/e/applications/all/%s", ecore_desktop_home_get(), key);
         if (!ecore_file_exists(path2))
            ecore_file_symlink(file, path2);
         ecore_sheap_insert(order_data->sheap, strdup(key));
     }
}

static void
_e_fdo_menu_to_order_add_sheap(Ecore_Sheap *sheap, const char *order_path, const char *file)
{
   char path2[PATH_MAX];
   FILE *f;

   if (file)
      ecore_sheap_insert(sheap, strdup(file));
   snprintf(path2, sizeof(path2), "%s/.order", order_path);
   f = fopen(path2, "r");
   if (f)
      {
        char buffer[PATH_MAX];

        /* Read all entries from existing order file, store in sheap for sorting. */
         while (fgets(buffer, sizeof(buffer), f) != NULL)
            {
               int length;

               /* Strip new line char */
               if (buffer[(length = strlen(buffer) - 1)] == '\n')
                  buffer[length] = '\0';
               ecore_sheap_insert(sheap, strdup(buffer));
            }
         fclose(f);
      }

   f = fopen(path2, "w");
   if (!f)
      fprintf(stderr, "ERROR: Cannot Open Order File %s \n", path2);
   else
      {
         char *last = NULL;
         int i;

         for (i = 0; i < sheap->size; i++)
            {
               char *item;

               item = ecore_sheap_item(sheap, i);
               if (item)
	          {
		     /* Weed out the dupes. */
		     if ((last) && (strcmp(last, item) == 0))
		        continue;
                     fprintf(f, "%s\n", item);
		     last = item;
		  }
	    }
         fclose(f);
      }
}
