// Auteur Adrien et Léni
// Programme test règles jeu d'échecs
// Config : Serial 115200 / PIN_LED à modifier selon Arduino UNO ou ESP32

//==============================================================
//  En-tête et inclusions
//==============================================================

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "A31301.h"
#include "config.h"



//==============================================================
// Configuration matérielle (LEDs)
//==============================================================

//------------------Constantes LEDs------------------

#define LED_PIN A4    // Broche GPIO : A4 (ESP32) ou 2 (Arduino UNO)
#define LED_COUNT 64  // Nombre de LEDs (une par case)

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

//------------------Fonctions de pilotage LEDs------------------

//Remplissage séquentiel (effet démo)
void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    delay(wait);
  }
}

//Allumer une case via le tableau de mapping
void setuLED(uint8_t addr_led, uint32_t color) {
  strip.setPixelColor(tab_LED[addr_led] - 1, color);  // tab_LED mappe index case -> index LED physique
}

//==============================================================
//  Modèle de données (échecs)
//==============================================================

//------------------Types et énumérations------------------

enum Couleur { VIDE,
               BLANC,
               NOIR };

enum TypePiece { AUCUN,
                 PION,
                 CAVALIER,
                 FOU,
                 TOUR,
                 DAME,
                 ROI };

//------------------Classe Piece------------------

// Représentation d'une pièce sur le plateau
class Piece {
private:
  TypePiece type;
  Couleur couleur;
  bool active;
  int x, y;            // Coordonnées (0 à 7), convention plateau[x][y]
  int nbDeplacements;  // Pour premier pas du pion (avance 2) et roque

public:
  // Constructeur par défaut : case vide
  Piece()
    : type(AUCUN), couleur(VIDE), active(false), x(-1), y(-1), nbDeplacements(0) {}

  // Constructeur complet : pièce en position
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

  void setActive(bool etat) { active = etat; }

  // Réinitialise la pièce (type, couleur, position, nb coups)
  void reset(TypePiece t, Couleur c, int posX, int posY) {
    type = t;
    couleur = c;
    x = posX;
    y = posY;
    active = true;
    nbDeplacements = 0;
  }

  // Vide la case (plus de pièce)
  void vider() {
    type = AUCUN;
    couleur = VIDE;
    active = false;
    nbDeplacements = 0;
  }
};

//------------------État global du plateau------------------

#define MAX_COUPS 40

void calculerDeplacements(Piece &p);
int genererCoupsPossibles(Piece &p, int X[], int Y[]);
void afficherCoupsPossibles(int X[], int Y[], int nbCoups);
void afficherPlateauSerial();

Piece plateau[8][8];  // plateau[x][y], x = colonne, y = ligne

//==============================================================
//  Initialisation (setup)
//==============================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    Serial.println("pas de serial");
  }
  Serial.println("Setup");

  Wire.begin();              // I2C pour magnétomètres A31301
  strip.begin();             // Communication NeoPixel
  strip.show();              // Éteint toutes les LEDs au démarrage
  strip.setBrightness(100);  // Luminosité ~20% (économie courant USB)
}

//==============================================================
//  Boucle principale (loop)
//==============================================================

void loop() {

  //------------------Lecture capteurs et envoi série------------------

  // En-tête trame (0xAA 0xBB = début paquet)
  Serial.write(0xAA);
  Serial.write(0xBB);
  uint8_t checksum = 0;
  int16_t ValeurZ = 0;
  uint8_t etat = 0;  // 0 = Noir, 1 = Blanc, 2 = Vide

  //Parcours des 64 cases du plateau
  for (uint8_t k = 0; k < 8; k++) {
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t indexCase = j + (k * 8);

      if (presence_pion_blanc(indexCase)) {
        setuLED(indexCase, strip.Color(255, 255, 255));  // LED blanche
        etat = 1;
        plateau[j][k].reset(ROI, BLANC, j, k);  // TODO : mapper type réel depuis capteur
        calculerDeplacements(plateau[j][k]);
      } else if (presence_pion_noir(indexCase)) {
        setuLED(indexCase, strip.Color(255, 255, 0));    // LED jaune
        etat = 0;
      } else {
        setuLED(indexCase, strip.Color(0, 0, 0));       // LED éteinte
        etat = 2;
      }

      ValeurZ = getZ(indexCase);
      uint8_t highZ = (ValeurZ >> 8) & 0xFF;
      uint8_t lowZ = ValeurZ & 0xFF;
      Serial.write(indexCase);
      Serial.write(highZ);
      Serial.write(lowZ);
      Serial.write(etat);
      checksum += (indexCase + highZ + lowZ + etat);
    }
  }
  Serial.write(checksum);

  //------------------Traitement des commandes série------------------

  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // Interroger une case (ex: "1,6") → affiche les coups possibles
    if (cmd.length() == 3) {
      int x = cmd.substring(0, 1).toInt();
      int y = cmd.substring(2, 3).toInt();
      if (plateau[x][y].getType() == AUCUN) {
        Serial.println("Case vide.");
      } else {
        Serial.print("Selection: ");
        calculerDeplacements(plateau[x][y]);
      }
    }

    // Commande visuelle (?) → affiche le plateau en texte
    if (cmd == "?") {
      afficherPlateauSerial();
    }

    // Déplacer (ex: "1,6 1,4") → origine vers destination
    else if (cmd.length() >= 7) {
      int x1 = cmd.substring(0, 1).toInt();
      int y1 = cmd.substring(2, 3).toInt();
      int x2 = cmd.substring(4, 5).toInt();
      int y2 = cmd.substring(6, 7).toInt();

      if (plateau[x1][y1].getType() != AUCUN) {
        plateau[x2][y2] = plateau[x1][y1];
        plateau[x2][y2].setPosition(x2, y2);
        plateau[x1][y1].vider();
        Serial.println("Coup effectue.");
      }
    }

    // Commande test (GO) → place un pion noir en 2,4
    if (cmd == "GO") {
      plateau[2][4].reset(PION, NOIR, 2, 4);
    }
  }
}

//==============================================================
//  Moteur de règles (calcul des déplacements)
//==============================================================

// Déclarations anticipées (générateurs par pièce)
int genererCoupsPion(Piece &p, int X[], int Y[]);
int genererCoupsFou(Piece &p, int X[], int Y[]);
int genererCoupsTour(Piece &p, int X[], int Y[]);
int genererCoupsDame(Piece &p, int X[], int Y[]);
int genererCoupsCavalier(Piece &p, int X[], int Y[]);
int genererCoupsRoi(Piece &p, int X[], int Y[]);
int ajouterCoupsDirections(Piece &p, const int dir[][2], int nbDir, int X[], int Y[]);

//------------------Point d'entrée principal------------------

// Dispatche vers le bon générateur selon le type de pièce, puis appelle l'affichage
void calculerDeplacements(Piece &p) {
  int X[MAX_COUPS];
  int Y[MAX_COUPS];
  int nbCoups = genererCoupsPossibles(p, X, Y);
  afficherCoupsPossibles(X, Y, nbCoups);
}

//------------------Génération des coups (logique pure, sans affichage)------------------

// Dispatche selon le type de pièce. Retourne le nombre de coups, remplit X[] et Y[].
int genererCoupsPossibles(Piece &p, int X[], int Y[]) {
  TypePiece t = p.getType();
  int nb = 0;

  switch (t) {
    case PION:   nb = genererCoupsPion(p, X, Y);   break;
    case FOU:    nb = genererCoupsFou(p, X, Y);    break;
    case TOUR:   nb = genererCoupsTour(p, X, Y);   break;
    case DAME:   nb = genererCoupsDame(p, X, Y);   break;
    case CAVALIER: nb = genererCoupsCavalier(p, X, Y); break;
    case ROI:    nb = genererCoupsRoi(p, X, Y);    break;
    default:     break;
  }
  return nb;
}

// Pion : avance 1 ou 2, captures diagonales
int genererCoupsPion(Piece &p, int X[], int Y[]) {
  int x = p.getX(), y = p.getY(), n = p.getNbDeplacements();
  Couleur c = p.getCouleur();
  int dirY = (c == BLANC) ? 1 : -1;
  int nb = 0;

  if (y + dirY < 0 || y + dirY > 7) return 0;

  if (plateau[x][y + dirY].getType() == AUCUN) {
    X[nb] = x; Y[nb] = y + dirY; nb++;
    if (n == 0 && plateau[x][y + 2 * dirY].getType() == AUCUN) {
      X[nb] = x; Y[nb] = y + 2 * dirY; nb++;
    }
  }
  for (int d = 0; d < 2; d++) {
    int dx = (d == 0) ? 1 : -1;
    int nx = x + dx;
    if (nx >= 0 && nx <= 7 && plateau[nx][y + dirY].getType() != AUCUN && plateau[nx][y + dirY].getCouleur() != c) {
      X[nb] = nx; Y[nb] = y + dirY; nb++;
    }
  }
  return nb;
}

// Fou : déplacements en diagonale (réutilise ajouterCoupsDirection)
int genererCoupsFou(Piece &p, int X[], int Y[]) {
  const int dir[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
  return ajouterCoupsDirections(p, dir, 4, X, Y);
}

// Tour : déplacements en ligne (réutilise ajouterCoupsDirection)
int genererCoupsTour(Piece &p, int X[], int Y[]) {
  const int dir[4][2] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
  return ajouterCoupsDirections(p, dir, 4, X, Y);
}

// Dame : Fou + Tour
int genererCoupsDame(Piece &p, int X[], int Y[]) {
  const int dir[8][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }, { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
  return ajouterCoupsDirections(p, dir, 8, X, Y);
}

// Cavalier : 8 cases en L
int genererCoupsCavalier(Piece &p, int X[], int Y[]) {
  const int dir[8][2] = { { -2, 1 }, { -1, 2 }, { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 } };
  int x = p.getX(), y = p.getY();
  Couleur c = p.getCouleur();
  int nb = 0;
  for (int d = 0; d < 8; d++) {
    int nx = x + dir[d][0], ny = y + dir[d][1];
    if (nx >= 0 && nx <= 7 && ny >= 0 && ny <= 7) {
      if (plateau[nx][ny].getType() == AUCUN || plateau[nx][ny].getCouleur() != c) {
        X[nb] = nx; Y[nb] = ny; nb++;
      }
    }
  }
  return nb;
}

// Roi : 8 cases autour (1 case par direction)
int genererCoupsRoi(Piece &p, int X[], int Y[]) {
  const int dir[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
  int x = p.getX(), y = p.getY();
  Couleur c = p.getCouleur();
  int nb = 0;
  for (int d = 0; d < 8; d++) {
    int nx = x + dir[d][0], ny = y + dir[d][1];
    if (nx >= 0 && nx <= 7 && ny >= 0 && ny <= 7) {
      if (plateau[nx][ny].getType() == AUCUN || plateau[nx][ny].getCouleur() != c) {
        X[nb] = nx; Y[nb] = ny; nb++;
      }
    }
  }
  return nb;
}

// Helper : parcourt une direction jusqu'à obstacle, ajoute les coups
int ajouterCoupsDirections(Piece &p, const int dir[][2], int nbDir, int X[], int Y[]) {
  int x = p.getX(), y = p.getY();
  Couleur c = p.getCouleur();
  int nb = 0;
  for (int d = 0; d < nbDir; d++) {
    for (int i = 1; i < 8; i++) {
      int nx = x + i * dir[d][0], ny = y + i * dir[d][1];
      if (nx < 0 || nx > 7 || ny < 0 || ny > 7) break;
      if (plateau[nx][ny].getType() == AUCUN) {
        X[nb] = nx; Y[nb] = ny; nb++;
      } else {
        if (plateau[nx][ny].getCouleur() != c) { X[nb] = nx; Y[nb] = ny; nb++; }
        break;
      }
    }
  }
  return nb;
}

//------------------Affichage des coups (à personnaliser)------------------

// Affiche les coups possibles : Serial + LEDs. Modifier ici pour changer l'affichage.
void afficherCoupsPossibles(int X[], int Y[], int nbCoups) {
  Serial.println("Coup possible:");
  for (int i = 0; i < nbCoups; i++) {
    Serial.print("X: ");
    Serial.print(X[i]);
    Serial.print(" | Y: ");
    Serial.println(Y[i]);
    setuLED(X[i] + Y[i] * 8, strip.Color(255, 0, 0));  // LED rouge = case atteignable
  }
  strip.show();
}

//==============================================================
//  Affichage et débogage
//==============================================================

//------------------Affichage plateau en texte (Serial)------------------

void afficherPlateauSerial() {
  Serial.println("\n    0    1    2    3    4    5    6    7   (X)");
  Serial.println("  +------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+");

  for (int y = 0; y < 8; y++) {
    Serial.print(y);
    Serial.print(" | ");
    for (int x = 0; x < 8; x++) {
      Piece &p = plateau[x][y];
      if (p.getType() == AUCUN) {
        Serial.print("  ");
      } else {
        char c;
        switch (p.getType()) {
          case PION: c = 'P'; break;
          case CAVALIER: c = 'C'; break;
          case FOU: c = 'F'; break;
          case TOUR: c = 'T'; break;
          case DAME: c = 'D'; break;
          case ROI: c = 'R'; break;
          default: c = ' '; break;
        }
        if (p.getCouleur() == NOIR) c = c + 32;  // Minuscule = Noir
        Serial.print(c);
        Serial.print((p.getCouleur() == BLANC) ? "b" : "n");
      }
      Serial.print(" | ");
    }
    Serial.println();
    Serial.println("  +------------------+------------------+------------------+------------------+------------------+------------------+------------------+------------------+");
  }
}
