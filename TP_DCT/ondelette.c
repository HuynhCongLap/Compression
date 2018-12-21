#include "bases.h"
#include "bitstream.h"
#include "sf.h"
#include "intstream.h"
#include "image.h"
#include "rle.h"
#include "exception.h"
#include "matrice.h"
#include "ondelette.h"

/*
 * Cette fonction effectue UNE SEULE itération d'une ondelette 1D
 * Voici quelques exemples de calculs
 *
 * Entree            Sortie
 * A                   A
 * A B               (A+B)/2 (A-B)/2
 * A B C             (A+B)/2    C    (A-B)/2
 * A B C D           (A+B)/2 (C+D)/2 (A-B)/2 (C-D)/2
 * A B C D E         (A+B)/2 (C+D)/2   E     (A-B)/2 (C-D)/2
 * A B C D E F       (A+B)/2 (C+D)/2 (E+F)/2 (A-B)/2 (C-D)/2 (E-F)/2
 */

void ondelette_1d(const float *entree, float *sortie, int nbe)
{

  int j=0;
  for(int i=0; i<(nbe/2); i++)
  {
    sortie[i] = (entree[j] + entree[j+1])/2.0;
    sortie[(nbe/2)+(nbe%2)+i] = (entree[j] - entree[j+1])/2.0;
    j+=2;
  }

  if(nbe%2)
  {
    sortie[nbe/2] = entree[nbe-1];
  }

}

/*
 * Comme pour la DCT, on applique dans un sens puis dans l'autre.
 *
 * La fonction reçoit "image" et la modifie directement.
 *
 * Vous pouvez allouer plusieurs images intermédiaires pour
 * simplifier la fonction.
 *
 * Par exemple pour une image  3x6
 *    3x6 ondelette horizontale
 *    On transpose, on a donc une image 6x3
 *    6x3 ondelette horizontale
 *    On transpose à nouveau, on a une image 3x6
 *    On ne travaille plus que sur les basses fréquences (moyennes)
 *    On ne travaille donc que sur le haut gauche de l'image de taille 2x3
 *
 * On recommence :
 *    2x3 horizontal
 *    transposee => 3x2
 *    3x2 horizontal
 *    transposee => 2x3
 *    basse fréquences => 1x2
 *
 * On recommence :
 *    1x2 horizontal
 *    transposee => 2x1
 *    2x1 horizontal (ne fait rien)
 *    transposee => 1x2
 *    basse fréquences => 1x1
 *
 * L'image finale ne contient qu'un seul pixel de basse fréquence.
 * Les autres sont des blocs de plus ou moins haute fréquence.
 * Sur une image 8x8 :
 *
 * M   	F1H  F2H  F2H  F3H  F3H  F3H  F3H
 * F1V 	F1HV F2H  F2H  F3H  F3H  F3H  F3H
 * F2V 	F2V  F2HV F2HV F3H  F3H  F3H  F3H
 * F2V 	F2V  F2HV F2HV F3H  F3H  F3H  F3H
 * F3V 	F3V  F3V  F3V  F3HV F3HV F3HV F3HV
 * F3V 	F3V  F3V  F3V  F3HV F3HV F3HV F3HV
 * F3V 	F3V  F3V  F3V  F3HV F3HV F3HV F3HV
 * F3V 	F3V  F3V  F3V  F3HV F3HV F3HV F3HV
 *
 * La fréquence F2 est plus petite (moins haute) que la fréquence F3
 * F1H  Indique que c'est une fréquence horizontale
 * F1V  Indique que c'est une fréquence verticale
 * F1HV Indique que c'est une fréquence calculée dans les 2 directions
 *
 */

void ondelette_2d(Matrice *image)
{
  int h = image->height;
  int w = image->width;

  while(h*w != 1)
  {
    Matrice *inter_1 = allocation_matrice_float(h,w);
    Matrice *inter_2 = allocation_matrice_float(w,h);
    Matrice *inter_3 = allocation_matrice_float(w,h);
    Matrice *inter_4 = allocation_matrice_float(h,w);

    for(int i=0; i<h; i++)
    ondelette_1d(image->t[i], inter_1->t[i], w);
    transposition_matrice_partielle(inter_1,inter_2,h,w);

    for(int i=0; i<w; i++)
    ondelette_1d(inter_2->t[i], inter_3->t[i], h);
    transposition_matrice_partielle(inter_3,inter_4,w,h);

    for(int i=0; i<h; i++)
    for(int j=0; j<w; j++)
      image->t[i][j] = inter_4->t[i][j];


    if(w != 1)
        w = round(w/2.f);

    if(h != 1)
        h = round(h/2.f);

    liberation_matrice_float(inter_1);
    liberation_matrice_float(inter_2);
    liberation_matrice_float(inter_3);
    liberation_matrice_float(inter_4);
  }
}

/*
 * Quantification de l'ondelette.
 * La facteur de qualité initial s'applique à la fréquence la plus haute.
 * Quand on divise la fréquence par 2 on divise qualité par 8
 * tout en restant supérieur à 1.
 * Une qualité de 1 indique que l'on a pas de pertes.
 */

void quantif_ondelette(Matrice *image, float qualite)
{

    for(int i=0; i <image->height; i++)
    for(int j=0; j <image->width; j++)
      image->t[i][j] = image->t[i][j]/(1+(i+j+1)*qualite/100);

}

/*
 * Sortie des coefficients dans le bonne ordre afin
 * d'être bien compressé par la RLE.
 * Cette fonction n'est pas optimale, elle devrait faire
 * un parcours de Péano sur chacun des blocs.
 */

void codage_ondelette(Matrice *image, FILE *f)
 {
  int j, i ;
  float *t, *pt ;
  struct intstream *entier, *entier_signe ;
  struct bitstream *bs ;
  struct shannon_fano *sf ;
  int hau, lar ;

  /*
   * Conversion de la matrice en une table linéaire
   * Pour pouvoir utiliser la fonction "compresse"
   */
  hau = image->height ;
  lar = image->width ;
  ALLOUER(t, hau*lar) ;
  pt = t ;

  while( hau != 1 || lar != 1 )
    {
      for(j=0; j<hau; j++)
	for(i=0; i<lar; i++)
	  if ( j>=(hau+1)/2 || i>=(lar+1)/2 )
	    *pt++ = image->t[j][i] ;

      hau = (hau+1)/2 ;
      lar = (lar+1)/2 ;
    }
  *pt = image->t[0][0] ;
  /*
   * Compression RLE avec Shannon-Fano
   */
  bs = open_bitstream("-", "w") ;
  sf = open_shannon_fano() ;
  entier = open_intstream(bs, Shannon_fano, sf) ;
  entier_signe = open_intstream(bs, Shannon_fano, sf) ;

  compresse(entier, entier_signe, image->height*image->width, t) ;

  close_intstream(entier) ;
  close_intstream(entier_signe) ;
  close_bitstream(bs) ;
  free(t) ;
 }



/*
*******************************************************************************
* Fonctions inverses
*******************************************************************************
*/

void ondelette_1d_inverse(const float *entree, float *sortie, int nbe)
{

  int j=0;
  for(int i=0; i<(nbe/2); i++)
  {
    sortie[j] = entree[i] + entree[(nbe/2)+(nbe%2)+i] ;
    sortie[j+1]= entree[i] - entree[(nbe/2)+(nbe%2)+i];
    j+=2;
  }
  if(nbe%2)
  {
    sortie[nbe-1]= entree[nbe/2];
  }

}


void ondelette_2d_inverse(Matrice *image)
{

  int h = image->height;
  int w = image->width;

  int count_w = 0;
  int count_h = 0;
  int count =0;
  while(h*w != 1)
  {
    count++;
    if(w != 1){
      count_w++;
      w=round(w/2.0);
    }

    if(h != 1){
      count_h++;
      h=round(h/2.0);
    }
  }
  h = image->height;
  w = image->width;

    while(count>0){
      int ww=w;
      int hh =h;
      for(int i=0; i<count-1 ; i++)
        hh =  round(hh/2.f);
      for(int i=0; i<count-1 ; i++)
        ww =  round(ww/2.f);
      //  count_h--; count_w--;

    //fprintf(stderr, "hh:%d ww:%d\n",hh,ww);
    //fprintf(stderr, "Hello\n");
    Matrice *inter_1 = allocation_matrice_float(hh,ww);
    Matrice *inter_2 = allocation_matrice_float(ww,hh);
    Matrice *inter_3 = allocation_matrice_float(ww,hh);
    Matrice *inter_4 = allocation_matrice_float(hh,ww);

    for(int i=0; i<hh; i++)
    ondelette_1d_inverse(image->t[i], inter_1->t[i], ww);
    transposition_matrice_partielle(inter_1,inter_2,hh,ww);

    for(int i=0; i<ww; i++)
    ondelette_1d_inverse(inter_2->t[i], inter_3->t[i], hh);
    transposition_matrice_partielle(inter_3,inter_4,ww,hh);

    for(int i=0; i<hh; i++)
    for(int j=0; j<ww; j++)
      image->t[i][j] = inter_4->t[i][j];



    count--;
    liberation_matrice_float(inter_1);
    liberation_matrice_float(inter_2);
    liberation_matrice_float(inter_3);
    liberation_matrice_float(inter_4);
  }

}


void dequantif_ondelette(Matrice *image, float qualite)
{
  for(int i=0; i <image->height; i++)
  for(int j=0; j <image->width; j++)
      image->t[i][j] = image->t[i][j]*(1+(i+j+1)*qualite/100);
}

void decodage_ondelette(Matrice *image, FILE *f)
 {
  int j, i ;
  float *t, *pt ;
  struct intstream *entier, *entier_signe ;
  struct bitstream *bs ;
  struct shannon_fano *sf ;
  int largeur = image->width, hauteur = image->height ;

  /*
   * Decompression RLE avec Shannon-Fano
   */
  ALLOUER(t, hauteur*largeur) ;
  bs = open_bitstream("-", "r") ;
  sf = open_shannon_fano() ;
  entier = open_intstream(bs, Shannon_fano, sf) ;
  entier_signe = open_intstream(bs, Shannon_fano, sf) ;

  decompresse(entier, entier_signe, hauteur*largeur, t) ;

  close_intstream(entier) ;
  close_intstream(entier_signe) ;
  close_bitstream(bs) ;

  /*
   * Met dans la matrice
   */
  pt = t ;
  while( hauteur != 1 || largeur != 1 )
    {
      for(j=0; j<hauteur; j++)
	for(i=0; i<largeur; i++)
	  if ( j>=(hauteur+1)/2 || i>=(largeur+1)/2 )
	      image->t[j][i] = *pt++ ;

      hauteur = (hauteur+1)/2 ;
      largeur = (largeur+1)/2 ;
    }
  image->t[0][0] = *pt++ ;

  free(t) ;
 }

/*
 * Programme de test.
 * La ligne suivante compile, compresse et décompresse l'image
 * et affiche la taille compressée.

export QUALITE=1  # Qualité de "quantification"
export SHANNON=1  # Si 1, utilise shannon-fano dynamique
ondelette <DONNEES/bat710.pgm 1 >xxx && ls -ls xxx && ondelette_inv <xxx | xv -

 */

void ondelette_encode_image(float qualite)
 {
  struct image *image ;
  Matrice *im ;
  int i, j ;

  image = lecture_image(stdin) ;
  assert(fwrite(&image->hauteur, 1, sizeof(image->hauteur), stdout)
	 == sizeof(image->hauteur)) ;
  assert(fwrite(&image->largeur, 1, sizeof(image->largeur), stdout)
	 == sizeof(image->largeur));
  assert(fwrite(&qualite       , 1, sizeof(qualite)       , stdout)
	 == sizeof(qualite));

  im = allocation_matrice_float(image->hauteur, image->largeur) ;
  for(j=0; j<image->hauteur; j++)
    for(i=0; i<image->largeur; i++)
      im->t[j][i] = image->pixels[j][i] ;

  fprintf(stderr, "Compression ondelette, image %dx%d\n"
	  , image->largeur, image->hauteur) ;
  ondelette_2d     (im) ;
  fprintf(stderr, "Quantification qualité = %g\n", qualite) ;
  quantif_ondelette(im, qualite) ;
  fprintf(stderr, "Codage\n") ;
  codage_ondelette(im, stdout) ;

  //  affiche_matrice_float(im, image->hauteur, image->largeur) ;
 }

void ondelette_decode_image()
 {
  int hauteur, largeur ;
  float qualite ;
  struct image *image ;
  Matrice *im ;

  assert(fread(&hauteur, 1, sizeof(hauteur), stdin) == sizeof(hauteur)) ;
  assert(fread(&largeur, 1, sizeof(largeur), stdin) == sizeof(largeur)) ;
  assert(fread(&qualite, 1, sizeof(qualite), stdin) == sizeof(qualite)) ;

  im = allocation_matrice_float(hauteur, largeur) ;

  fprintf(stderr, "Décodage\n") ;
  decodage_ondelette(im, stdin ) ;

  fprintf(stderr, "Déquantification qualité = %g\n", qualite) ;
  dequantif_ondelette(im, qualite) ;

  fprintf(stderr, "Décompression ondelette, image %dx%d\n", largeur, hauteur) ;
  ondelette_2d_inverse (im) ;

  //  affiche_matrice_float(im, hauteur, largeur) ;
  image = creation_image_a_partir_de_matrice_float(im) ;
  ecriture_image(stdout, image) ;
 }
