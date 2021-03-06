#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <time.h>

#define bool int

//-----------------------------------------------------------------------------
// Déclaration des types
//-----------------------------------------------------------------------------
/**
   Le contexte contient les informations utiles de l'interface pour
   les algorithmes de traitement d'image.  
*/
typedef struct SContexte {
  int width;
  int height;
  GdkPixbuf* pixbuf_input;
  GdkPixbuf* pixbuf_output;
  GtkWidget* image;
  GtkWidget* seuil;
  GtkWidget* floue;
} Contexte;

/**
   Un pixel est une structure de 3 octets (rouge, vert, bleu). On les
   plaque au bon endroit dans un pixbuf pour modifier les couleurs du pixel.
 */
typedef struct {
  guchar rouge;
  guchar vert;
  guchar bleu;
} Pixel;

/**
   Un Objet est un un wrapper d'un pixel permettant l'organisation en forêt.
   Un pixel a maintenant un rang et un père.
   Un ancètre dont le père est lui même s'appelle la racine.
 */
typedef struct SObjet {
  Pixel* pixel; // adresse du pixel dans le pixbuf
  int rang;
  struct SObjet* pere;
} Objet;

/**
  structure pour stocker la moyenne des couleurs
*/
typedef struct {
  double rouge;
  double vert;
  double bleu;
  int nb;
} StatCouleur;

/**
  structure de couleur au format TSV
*/
typedef struct {
  int t;    // teinte comme angle en degrés
  double s; // saturation comme nombre entre 0 et 1
  double v; // valeur ou brillance comme nombre entre 0 et 1
} TSVCouleur;

//-----------------------------------------------------------------------------
// Déclaration des fonctions
//-----------------------------------------------------------------------------
gboolean selectInput( GtkWidget *widget, gpointer data );
gboolean selectOutput( GtkWidget *widget, gpointer data );
gboolean seuillerImage( GtkWidget *widget, gpointer data );
gboolean composantesConnexes( GtkWidget *widget, gpointer data );
gboolean composantesConnexesFloues( GtkWidget *widget, gpointer data );
GtkWidget* creerIHM( const char* image_filename, Contexte* pCtxt );
void analyzePixbuf( GdkPixbuf* pixbuf );
GdkPixbuf* creerImage( int width, int height );
unsigned char greyLevel( Pixel* data );
void setGreyLevel( Pixel* data, unsigned char g );
Pixel* gotoPixel( GdkPixbuf* pixbuf, int x, int y );
void disk( GdkPixbuf* pixbuf, int r );

Objet* creerEnsembles( GdkPixbuf* pixbuf );
Objet* trouverPasOpti( Objet* obj );
void unionPasOpti( Objet* obj1, Objet* obj2 );
Objet* trouverOpti( Objet* obj );
void unionOpti( Objet* obj1, Objet* obj2 );
TSVCouleur tsv( Pixel* pixel );
double similitude( Pixel* p1, Pixel* p2 );


//-----------------------------------------------------------------------------
// Programme principal
//-----------------------------------------------------------------------------
int main( int   argc,
          char* argv[] )
{
  Contexte context;
  const char* image_filename = argc > 1 ? argv[ 1 ] : "lena.png";

  /* Passe les arguments à GTK, pour qu'il extrait ceux qui le concernent. */
  gtk_init( &argc, &argv );
  
  /* Crée une fenêtre. */
  creerIHM( image_filename, &context );

  /* Rentre dans la boucle d'événements. */
  gtk_main ();
  return 0;
}


/// Fonction appelée lorsqu'on clique sur "Input".
gboolean selectInput( GtkWidget *widget, gpointer data )
{
  // Récupère le contexte.
  Contexte* pCtxt = (Contexte*) data;
  // Place le pixbuf à visualiser dans le bon widget.
  gtk_image_set_from_pixbuf( GTK_IMAGE( pCtxt->image ), pCtxt->pixbuf_input );
  // Force le réaffichage du widget.
  gtk_widget_queue_draw( pCtxt->image );
  return TRUE;
}

/// Fonction appelée lorsqu'on clique sur "Output".
gboolean selectOutput( GtkWidget *widget, gpointer data )
{
  // Récupère le contexte.
  Contexte* pCtxt = (Contexte*) data;
  // Place le pixbuf à visualiser dans le bon widget.
  gtk_image_set_from_pixbuf( GTK_IMAGE( pCtxt->image ), pCtxt->pixbuf_output );
  // Force le réaffichage du widget.
  gtk_widget_queue_draw( pCtxt->image );
  return TRUE;
}

gboolean seuillerImage( GtkWidget *widget, gpointer data )
{
  Contexte* ctx = (Contexte*) data;
  guchar* dataInput = gdk_pixbuf_get_pixels( ctx->pixbuf_input );          // Pointeur vers le tampon de données
  guchar* dataOutput = gdk_pixbuf_get_pixels( ctx->pixbuf_output );          // Pointeur vers le tampon de données
  int rowstrideIn = gdk_pixbuf_get_rowstride( ctx->pixbuf_input ); // nmb octets dans la ligne
  int rowstrideOut = gdk_pixbuf_get_rowstride( ctx->pixbuf_output ); // nmb octets dans la ligne
  int seuilValue = gtk_range_get_value( GTK_RANGE( ctx->seuil ) );

  for ( int y = 0; y < ctx->height; ++y )
  {
    Pixel* pixelIn = (Pixel*) dataInput;
    Pixel* pixelOut = (Pixel*) dataOutput;
    for ( int x = 0; x < ctx->width; ++x ) 
      {
        if (seuilValue > greyLevel(pixelIn))
          setGreyLevel(pixelOut,0);
        else
          setGreyLevel(pixelOut,255);

        ++pixelIn;
        ++pixelOut;
      }
    dataOutput += rowstrideOut; // passe à la ligne suivante
    dataInput += rowstrideIn;
  }
  return TRUE;
}

gboolean composantesConnexes( GtkWidget *widget, gpointer data )
{
  Contexte *ctx = (Contexte*) data;
  // 1
  Objet *objects = creerEnsembles(ctx->pixbuf_output);
  int size = ( ctx->width ) * ( ctx->height );

  for(int i = 0 /*2*/; i < size; i++)
  {
    // 3a
    bool isLastColumn = (i % ctx->width) == ctx->width - 1;
    if(!isLastColumn && greyLevel(objects[i].pixel) == greyLevel(objects[i+1].pixel))
      unionOpti(&objects[i], &objects[i+1]);
    // 3b
    bool isLastLine = i >= (size - ctx-> width);
    if( !isLastLine && greyLevel(objects[i].pixel) == greyLevel(objects[i+ctx->width].pixel))
      unionOpti(&objects[i],&objects[i+ctx->width]);
  }

  // 4 & 5
  StatCouleur *stats = (StatCouleur*) calloc( 4, size * sizeof(StatCouleur) );

  // 6
  guchar* data_input  = gdk_pixbuf_get_pixels( ctx ->pixbuf_input );
  guchar* data_output = gdk_pixbuf_get_pixels( ctx ->pixbuf_output );
  for ( int i = 0; i < size; ++i )
  {
    Objet* rep = trouverOpti( &objects[ i ] ); // tu trouve le représentant
    long int j = rep - objects; // tu trouve sons indice dans les tableaux
    Pixel* pixel_src = (Pixel*) ( data_input + ( (guchar*) objects[ i ].pixel - data_output ) ); 
    // pixel_src est la couleur de ce pixel dans l'image input.
    // On l'ajoute à la stat du représentant j.
    stats[ j ].rouge += pixel_src->rouge;
    stats[ j ].vert  += pixel_src->vert;
    stats[ j ].bleu  += pixel_src->bleu;
    stats[ j ].nb += 1; // On aura donc la somme cumulée
  }

  // 7
  for ( int i = 0; i < size; ++i )
  {
    // C'est un représentant
    if (stats[ i ].nb != 0) {
      objects[ i ].pixel->rouge = stats[ i ].rouge / stats[ i ].nb;
      objects[ i ].pixel->vert  = stats[ i ].vert  / stats[ i ].nb;
      objects[ i ].pixel->bleu  = stats[ i ].bleu  / stats[ i ].nb;
    }
  }

  free(stats);

  // 8
  for ( int i = 0; i < size; ++i )
  {
    Objet* rep = trouverOpti( &objects[ i ] ); // tu trouve le représentant
    objects[ i ].pixel->rouge = rep->pixel->rouge;
    objects[ i ].pixel->bleu  = rep->pixel->bleu;
    objects[ i ].pixel->vert  = rep->pixel->vert;
  }

  // Place le pixbuf à visualiser dans le bon widget.
  gtk_image_set_from_pixbuf( GTK_IMAGE( ctx->image ), ctx->pixbuf_output );
  // Force le réaffichage du widget.
  gtk_widget_queue_draw( ctx->image );

  return TRUE;
}

gboolean composantesConnexesFloues( GtkWidget *widget, gpointer data )
{
  Contexte *ctx = (Contexte*) data;

  // copie par valeur
  ctx->pixbuf_output = gdk_pixbuf_copy(ctx->pixbuf_input);

  double floue = gtk_range_get_value( GTK_RANGE( ctx->floue ) );
  // 1
  Objet *objects = creerEnsembles(ctx->pixbuf_output);
  int size = ( ctx->width ) * ( ctx->height );

  for(int i = 0 /*2*/; i < size; i++)
  {
    // 3a
    bool isLastColumn = (i % ctx->width) == ctx->width - 1;
    // printf("sim : %f\n",similitude(objects[ i ].pixel, objects[ i+1 ].pixel));
    if(!isLastColumn && similitude(objects[ i ].pixel, objects[ i+1 ].pixel) <= floue)
      unionOpti(&objects[ i ], &objects[ i+1 ]);
    // 3b
    bool isLastLine = i >= (size - ctx-> width);
    if( !isLastLine && similitude(objects[ i ].pixel, objects[ i + ctx->width ].pixel) <= floue)
      unionOpti(&objects[ i ], &objects[ i + ctx->width ]);
  }

  // 4 & 5
  StatCouleur *stats = (StatCouleur*) calloc( 4, size * sizeof(StatCouleur) );

  // 6
  guchar* data_input  = gdk_pixbuf_get_pixels( ctx ->pixbuf_input );
  guchar* data_output = gdk_pixbuf_get_pixels( ctx ->pixbuf_output );

  for ( int i = 0; i < size; ++i )
  {
    Objet* rep = trouverOpti( &objects[ i ] ); // tu trouve le représentant
    long int j = rep - objects; // tu trouve sons indice dans les tableaux
    Pixel* pixel_src = (Pixel*) ( data_input + ( (guchar*) objects[ i ].pixel - data_output ) ); 
    // pixel_src est la couleur de ce pixel dans l'image input.
    // On l'ajoute à la stat du représentant j.
    stats[ j ].rouge += pixel_src->rouge;
    stats[ j ].vert  += pixel_src->vert;
    stats[ j ].bleu  += pixel_src->bleu;
    stats[ j ].nb += 1; // On aura donc la somme cumulée
  }

  // 7
  for ( int i = 0; i < size; ++i )
  {
    // C'est un représentant
    if (stats[ i ].nb != 0) {
      objects[ i ].pixel->rouge = stats[ i ].rouge / stats[ i ].nb;
      objects[ i ].pixel->vert  = stats[ i ].vert  / stats[ i ].nb;
      objects[ i ].pixel->bleu  = stats[ i ].bleu  / stats[ i ].nb;
    }
  }

  free(stats);

  // 8
  for ( int i = 0; i < size; ++i )
  {
    Objet* rep = trouverOpti( &objects[ i ] ); // tu trouve le représentant
    objects[ i ].pixel->rouge = rep->pixel->rouge;
    objects[ i ].pixel->bleu  = rep->pixel->bleu;
    objects[ i ].pixel->vert  = rep->pixel->vert;
  }


  // struct timespec current; // Stoppe l'horloge
  // clock_gettime(CLOCK_REALTIME, &current); //Linux gettime
  // double t = (( current.tv_sec - myTimerStart.tv_sec) *1000 +
  //          ( current.tv_nsec - myTimerStart.tv_nsec)/1000000.0);
  // printf( "time = %lf ms.\n", t );

  // Place le pixbuf à visualiser dans le bon widget.
  gtk_image_set_from_pixbuf( GTK_IMAGE( ctx->image ), ctx->pixbuf_output );
  // Force le réaffichage du widget.
  gtk_widget_queue_draw( ctx->image );

  return TRUE;
}

/// Charge l'image donnée et crée l'interface.
GtkWidget* creerIHM( const char* image_filename, Contexte* pCtxt )
{
  GtkWidget* window;
  GtkWidget* vbox1;
  GtkWidget* vbox2;
  GtkWidget* hbox1;
  GtkWidget* button_quit;
  GtkWidget* button_select_input;
  GtkWidget* button_select_output;
  GtkWidget* seuil_widget;
  GtkWidget* floue_widget;
  GtkWidget* floue_button;
  GtkWidget* seuil_button;
  GtkWidget* connexe_button;
  GError**   error = NULL;

  /* Crée une fenêtre. */
  window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  // Crée un conteneur horitzontal box.
  hbox1 = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 10 );
  // Crée deux conteneurs vertical box.
  vbox1 = gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
  vbox2 = gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );

  // Creer la bar pour le seuil
  seuil_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 255, 1 );
  pCtxt->seuil = seuil_widget;
  floue_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 255, 1 );
  pCtxt->floue = floue_widget;

  // Crée le pixbuf source et le pixbuf destination
  pCtxt->pixbuf_input  = gdk_pixbuf_new_from_file( image_filename, error );
  pCtxt->width         = gdk_pixbuf_get_width( pCtxt->pixbuf_input );           // Largeur de l'image en pixels
  pCtxt->height        = gdk_pixbuf_get_height( pCtxt->pixbuf_input );          // Hauteur de l'image en pixels
  pCtxt->pixbuf_output = creerImage( pCtxt->width, pCtxt->height );
  analyzePixbuf( pCtxt->pixbuf_input );
  disk( pCtxt->pixbuf_output, 100 );
  // Crée le widget qui affiche le pixbuf image.
  pCtxt->image = gtk_image_new_from_pixbuf( pCtxt->pixbuf_input );
  // Rajoute l'image dans le conteneur hbox.
  gtk_container_add( GTK_CONTAINER( hbox1 ), pCtxt->image );
  // Rajoute le 2eme vbox dans le conteneur hbox (pour mettre les boutons sélecteur d'image).
  gtk_container_add( GTK_CONTAINER( hbox1 ), vbox2 );
  // Crée les boutons de sélection "source"/"destination".
  button_select_input  = gtk_button_new_with_label( "Input" );
  button_select_output = gtk_button_new_with_label( "Output" );
  // Creer le bouton seuil
  seuil_button = gtk_button_new_with_label( "Seuiller");
  floue_button = gtk_button_new_with_label( "Composantes connexes floues");

  connexe_button = gtk_button_new_with_label( "Composantes connexes" );
  // Connecte la réaction gtk_main_quit à l'événement "clic" sur ce bouton.
  g_signal_connect( button_select_input, "clicked",
                    G_CALLBACK( selectInput ),
                    pCtxt );
  g_signal_connect( button_select_output, "clicked",
                    G_CALLBACK( selectOutput ),
                    pCtxt );
  g_signal_connect( seuil_button, "clicked",
                    G_CALLBACK(seuillerImage),
                    pCtxt );
  g_signal_connect( connexe_button, "clicked",
                    G_CALLBACK(composantesConnexes),
                    pCtxt );
  g_signal_connect( floue_button, "clicked",
                    G_CALLBACK(composantesConnexesFloues),
                    pCtxt );
  gtk_container_add( GTK_CONTAINER( vbox2 ), button_select_input );
  gtk_container_add( GTK_CONTAINER( vbox2 ), button_select_output );
  // Crée le bouton quitter.
  button_quit = gtk_button_new_with_label( "Quitter" );
  // Connecte la réaction gtk_main_quit à l'événement "clic" sur ce bouton.
  g_signal_connect( button_quit, "clicked",
                    G_CALLBACK( gtk_main_quit ),
                    NULL);
  // Rajoute tout dans le conteneur vbox.
  gtk_container_add( GTK_CONTAINER( vbox1 ), hbox1 );
  gtk_container_add( GTK_CONTAINER( vbox1 ), seuil_widget );
  gtk_container_add( GTK_CONTAINER( vbox1 ), seuil_button );
  gtk_container_add( GTK_CONTAINER( vbox1 ), connexe_button );

  gtk_container_add( GTK_CONTAINER( vbox1 ), floue_widget );
  gtk_container_add( GTK_CONTAINER( vbox1 ), floue_button );

  gtk_container_add( GTK_CONTAINER( vbox1 ), button_quit );
  // Rajoute la vbox  dans le conteneur window.
  gtk_container_add( GTK_CONTAINER( window ), vbox1 );

  // Rend tout visible
  gtk_widget_show_all( window );

  return window;
}

/** 
    Utile pour vérifier que le GdkPixbuf a un formal usuel: 3 canaux RGB, 24 bits par pixel,
    et que la machine supporte l'alignement de la structure sur 3 octets.
*/
void analyzePixbuf( GdkPixbuf* pixbuf )
{
  int n_channels      = gdk_pixbuf_get_n_channels( pixbuf );      // Nb de canaux (Rouge, Vert, Bleu, potentiellement Alpha)
  int has_alpha       = gdk_pixbuf_get_has_alpha( pixbuf );       // Dit s'il y a un canal Alpha (transparence).
  int bits_per_sample = gdk_pixbuf_get_bits_per_sample( pixbuf ); // Donne le nombre de bits par échantillon (8 bits souvent).
  guchar* data        = gdk_pixbuf_get_pixels( pixbuf );          // Pointeur vers le tampon de données
  int width           = gdk_pixbuf_get_width( pixbuf );           // Largeur de l'image en pixels
  int height          = gdk_pixbuf_get_height( pixbuf );          // Hauteur de l'image en pixels
  int rowstride       = gdk_pixbuf_get_rowstride( pixbuf );       // Nombre d'octets entre chaque ligne dans le tampon de données
  printf( "n_channels = %d\n", n_channels );
  printf( "has_alpha  = %d\n", has_alpha );
  printf( "bits_per_sa= %d\n", bits_per_sample );
  printf( "width      = %d\n", width );
  printf( "height     = %d\n", height );
  printf( "data       = %p\n", (void*)data );
  printf( "rowstride  = %d\n", rowstride );
  Pixel*  pixel = (Pixel*) data;
  printf( "sizeof(Pixel)=%ld\n", sizeof(Pixel) );
  size_t diff = ((guchar*) (pixel+1)) - (guchar*) pixel;
  printf( "(pixel+1) - pixel=%ld\n", diff );
  assert( n_channels == 3 );
  assert( has_alpha == FALSE );
  assert( bits_per_sample == 8 );
  assert( sizeof(Pixel) == 3 );
  assert( diff == 3 );
}

/**
   Crée un image vide de taille width x height.
*/
GdkPixbuf* creerImage( int width, int height )
{
  GdkPixbuf* img = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, 8, width, height );
  return img;
}

/**
   Retourne le niveau de gris du pixel.
*/
unsigned char greyLevel( Pixel* data )
{
  return (data->rouge+data->vert+data->bleu)/3;
}

/**
   Met le pixel au niveau de gris \a g.
*/
void setGreyLevel( Pixel* data, unsigned char g )
{
  data->rouge = g;
  data->vert  = g;
  data->bleu  = g;
}

/** 
    Va au pixel de coordonnées (x,y) dans le pixbuf.
*/
Pixel* gotoPixel( GdkPixbuf* pixbuf, int x, int y )
{
   int rowstride = gdk_pixbuf_get_rowstride( pixbuf );
   guchar* data  = gdk_pixbuf_get_pixels( pixbuf );
   return (Pixel*)( data + y*rowstride + x*3 );
}

/**
   Crée une image sous forme de disque.
*/
void disk( GdkPixbuf* pixbuf, int r )
{
  int x,y;
  int width           = gdk_pixbuf_get_width( pixbuf );           // Largeur de l'image en pixels
  int height          = gdk_pixbuf_get_height( pixbuf );          // Hauteur de l'image en pixels
  int x0 = width/2;
  int y0 = height/2;
  for ( y = 0; y < height; ++y )
    {
      Pixel* pixel = gotoPixel( pixbuf, 0, y ); // les lignes peuvent être réalignées
      for ( x = 0; x < width; ++x )
        {
          int d2 = (x-x0)*(x-x0)+(y-y0)*(y-y0);
          if ( d2 >= r*r ) setGreyLevel( pixel, 0 );
          else setGreyLevel( pixel, 255-(int) sqrt(d2));
          ++pixel; // sur une ligne, les pixels se suivent
        }
    }
}

/**
   Creer un objet par pixel
 */
Objet* creerEnsembles( GdkPixbuf* pixbuf ) 
{
  int rowstride = gdk_pixbuf_get_rowstride( pixbuf );
  guchar* data  = gdk_pixbuf_get_pixels( pixbuf );
  int width     = gdk_pixbuf_get_width( pixbuf );
  int height    = gdk_pixbuf_get_height( pixbuf );

  Objet *objects = (Objet*) malloc(width*height*sizeof(Objet));
  int cpt_obj = 0;

  for ( int y = 0; y < height; ++y )
  {
    Pixel* pixel = (Pixel*) data;
    for ( int x = 0; x < width; ++x ) 
    {
      objects[cpt_obj].pixel = pixel;
      objects[cpt_obj].rang = 1;
      objects[cpt_obj].pere = &objects[cpt_obj];

      pixel++;
      cpt_obj++;
    }
    data += rowstride;
  }

  return objects;
}

Objet* trouverPasOpti( Objet* object )
{
  if(object == object->pere)
  {
    return object;
  }
  return trouverPasOpti( object->pere );
}

Objet* trouverOpti( Objet* obj )
{
  if (obj != obj->pere)
    obj->pere = trouverOpti( obj->pere);

  return obj->pere;
}

void unionPasOpti(Objet* obj1, Objet* obj2)
{
  Objet* u = trouverPasOpti( obj1 );
  Objet* y = trouverPasOpti( obj2 );
  u->pere = y;
}

void unionOpti( Objet* obj1, Objet* obj2 )
{
  Objet* u = trouverOpti(obj1);
  Objet* v = trouverOpti(obj2);

  if (u->rang > v->rang)
    v->pere = u;
  else 
    u->pere = v;
  
  if (u->rang == v->rang)
    v->rang = v->rang + 1;
}

TSVCouleur tsv(Pixel* pixel)
{
  TSVCouleur tsv;

  guchar max = fmaxf(fmaxf(pixel->bleu, pixel->rouge), pixel->vert);
  guchar min = fminf(fminf(pixel->bleu, pixel->rouge), pixel->vert);

  // pour t
  if (min == max)
  {
    tsv.t = 0;
  }
  else if (max == pixel->rouge)
  {
    tsv.t = ( 60 * (pixel->vert - pixel->bleu) / (max - min) + 360 ) % 360;
  }
  else if (max == pixel->vert)
  {
    tsv.t = 60 * ((pixel->bleu - pixel->rouge) / (max - min)) + 120;
  }
  else if (max == pixel->bleu)
  {
    tsv.t = 60 * ((pixel->rouge - pixel->vert) / (max - min)) + 240;
  }

  // pour s
  if (max == 0)
  {
    tsv.s = 0;
  }
  else
  {
    tsv.s = 1 - (min / max);
  }

  // pour v
  tsv.v = max;

  return tsv;
}

double similitude( Pixel* p1, Pixel* p2 ) 
{
  TSVCouleur tsv1 = tsv( p1 );
  TSVCouleur tsv2 = tsv( p2 );
  int diff = tsv1.t - tsv2.t;
  while ( diff >= 180 ) diff -= 360;
  while ( diff <= -180 ) diff += 360;
  return abs( (double) diff ) + 5.0 * abs( tsv1.s - tsv2.s ) + 10.0 * abs( tsv1.v - tsv2.v );
}