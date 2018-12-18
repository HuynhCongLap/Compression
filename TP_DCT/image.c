#include "image.h"


/*
 * Lecture d'une ligne du fichier.
 * On saute les lignes commençant par un "#" (commentaire)
 * On simplifie en considérant que les lignes ne dépassent pas MAXLIGNE
 */

void lire_ligne(FILE *f, char *ligne)
{
  char* buff;
  size_t len = 0;
  getline(&buff, &len, f);

  while(buff[0] == '#')
    getline(&buff, &len, f);
  strcpy(ligne,buff);
  free(buff);

}

/*
 * Allocation d'une image
 */

struct image* allocation_image(int hauteur, int largeur)
{
  struct image* img = NULL;
  ALLOUER(img, 1) ;

  img->largeur = largeur;
  img->hauteur = hauteur;
  ALLOUER(img->pixels, largeur) ;

  for (int i=0; i<largeur; i++)
         ALLOUER(img->pixels[i],  hauteur) ;

  return img ;

}

/*
 * Libération image
 */

void liberation_image(struct image* image)
{
  for (int i=0; i< image->largeur; i++)
  		free(image->pixels[i]);

			free(image->pixels);
			free(image);
}

/*
 * Allocation et lecture d'un image au format PGM.
 * (L'entête commence par "P5\nLargeur Hauteur\n255\n"
 * Avec des lignes de commentaire possibles avant la dernière.
 */

struct image* lecture_image(FILE *f)
{






































return 0 ; /* pour enlever un warning du compilateur */
}

/*
 * Écriture de l'image (toujours au format PGM)
 */

void ecriture_image(FILE *f, const struct image *image)
{










}
