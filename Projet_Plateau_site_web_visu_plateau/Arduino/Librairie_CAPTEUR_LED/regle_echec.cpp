#include "regle_echec.h"
#include "Arduino.h"


enum Couleur { BLANC, NOIR };
enum TypePiece {AUCUNE, PION, CAVALIER, FOU, TOUR, DAME, ROI };

class Piece {
  private:
    TypePiece type;
    Couleur couleur;
    bool active;
    int x, y;          // Coordonnées (0 à 7)
    int nbDeplacements; // Pour gérer le premier pas du pion et le roque

  public:
    // Constructeur par défaut
    Piece() : type(AUCUN), couleur(VIDE), active(false), x(-1), y(-1), nbDeplacements(0) {}

    // Constructeur complet
    Piece(TypePiece t, Couleur c, int posX, int posY) {
      type = t;
      couleur = c;
      x = posX;
      y = posY;
      active = true;
      nbDeplacements = 0;
    }

    // --- Getters ---
    TypePiece getType() { return type; }
    Couleur getCouleur() { return couleur; }
    bool estActive() { return active; }
    int getX() { return x; }
    int getY() { return y; }
    int getNbDeplacements() { return nbDeplacements; }

    // --- Setters ---
    void setPosition(int newX, int newY) {
      x = newX;
      y = newY;
      nbDeplacements++;
    }

    void setActive(bool etat) {
      active = etat;
    }

    // Utile pour le reset du jeu
    void reset(TypePiece t, Couleur c, int posX, int posY) {
      type = t;
      couleur = c;
      x = posX;
      y = posY;
      active = true;
      nbDeplacements = 0;
    }
    void vider() {
      type = AUCUN;
      couleur = VIDE;
      active = false;
      nbDeplacements = 0;
      // x et y peuvent rester ou être mis à -1
    }
};

void init_Partie(void){
  // --- ÉTAPE 1 : TOUT SUPPRIMER ---
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      plateau[i][j].vider(); 
    }
  }
  // --- ÉTAPE 2 : ajoute les pièces ---
  // Exemple : Placer les tours blanches
  plateau[0][0].reset(TOUR, BLANC, 0, 0);
  plateau[7][0].reset(TOUR, BLANC, 7, 0);

  plateau[1][0].reset(CAVALIER, BLANC, 1, 0);
  plateau[6][0].reset(CAVALIER, BLANC, 6, 0);

  plateau[2][0].reset(FOU, BLANC, 2, 0);
  plateau[5][0].reset(FOU, BLANC, 5, 0);

  plateau[3][0].reset(ROI, BLANC, 3, 0);
  plateau[4][0].reset(DAME, BLANC, 4, 0);
  // Exemple : Placer les pions noirs
  for(int i = 0; i < 8; i++) {
    plateau[i][1].reset(PION, BLANC, i, 1);
  }

  // Exemple : Placer les tours blanches
  plateau[0][7].reset(TOUR, NOIR, 0, 0);
  plateau[7][7].reset(TOUR, NOIR, 7, 0);

  plateau[1][7].reset(CAVALIER, NOIR, 1, 0);
  plateau[6][7].reset(CAVALIER, NOIR, 6, 0);

  plateau[2][7].reset(FOU, NOIR, 2, 0);
  plateau[5][7].reset(FOU, NOIR, 5, 0);

  plateau[3][7].reset(ROI, NOIR, 3, 0);
  plateau[4][7].reset(DAME, NOIR, 4, 0);
  // Exemple : Placer les pions noirs
  for(int i = 0; i < 8; i++) {
    plateau[i][1].reset(PION, NOIR, i, 1);
  }
}

// Cette fonction pourrait être dans votre classe ou en dehors
void calculerDeplacementsPion(Piece &p, Piece plateau[8][8]) {
  int x = p.getX();
  int y = p.getY();
  int direction = (p.getCouleur() == BLANC) ? -1 : 1; 

  Serial.print("Analyse Pion en ["); Serial.print(x); Serial.print(","); Serial.print(y); Serial.println("]");

  // 1. Avance d'une case
  int nextY = y + direction;
  if (nextY >= 0 && nextY <= 7) {
    if (plateau[x][nextY].getType() == AUCUN) {
      allumerLED(x, nextY);
      Serial.print("  -> Case libre detectee en ["); Serial.print(x); Serial.print(","); Serial.print(nextY); Serial.println("]");

      // 2. Avance de deux cases (premier coup)
      if (p.getNbDeplacements() == 0) {
        int doubleNextY = y + (2 * direction);
        if (doubleNextY >= 0 && doubleNextY <= 7 && plateau[x][doubleNextY].getType() == AUCUN) {
          allumerLED(x, doubleNextY);
          Serial.print("  -> Premier coup : double pas possible en ["); Serial.print(x); Serial.print(","); Serial.print(doubleNextY); Serial.println("]");
        }
      }
    }

    // 3. Captures en diagonale
    int diagX[] = {x - 1, x + 1};
    for (int dx : diagX) {
      if (dx >= 0 && dx <= 7) {
        Piece cible = plateau[dx][nextY];
        // On vérifie si la case contient un ennemi
        if (cible.getType() != AUCUN && cible.getCouleur() != p.getCouleur()) {
          allumerLED(dx, nextY);
          Serial.print("  !!! CAPTURE possible en ["); Serial.print(dx); Serial.print(","); Serial.print(nextY); Serial.println("]");
        }
      }
    }
  }
}







