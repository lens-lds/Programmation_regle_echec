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

// Prise en passant : case cible (où le pion atterrit en prenant en passant). -1 = aucune.
int enPassantCol = -1; // colonne de la case cible
int enPassantRow = -1; // ligne de la case cible

void calculerDeplacements(Piece &p);
int genererCoupsPossibles(Piece &p, int X[], int Y[]);
void afficherCoupsPossibles(int X[], int Y[], int nbCoups);
void afficherPlateauSerial();
void afficherPlateauLED();       // Met à jour les LEDs selon l'état du plateau (pour test)
void initPositionEnPassant();    // Position de test : pion blanc e2, pion noir d4
void appliquerCoup(int x1, int y1, int x2, int y2);  // Applique un coup + gestion en passant

// À appeler par le binôme quand il applique un coup :
// - Après un double pas du pion : setEnPassantTarget(col, row) avec la case où un pion adverse peut atterrir en prenant en passant.
// - Sinon (coup normal ou tour adverse) : clearEnPassantTarget().
// - Prise en passant : quand le coup joué est une prise en passant, en plus du déplacement, retirer le pion capturé (à (colDest, rowDest - dirY) pour le camp qui vient de jouer).
// - Roque : si le roi va en (6, ligneRoi) ou (2, ligneRoi), déplacer aussi la tour (7,ligneRoi)->(5,ligneRoi) ou (0,ligneRoi)->(3,ligneRoi).
void setEnPassantTarget(int col, int row);
void clearEnPassantTarget();

Piece plateau[8][8];  // plateau[x][y], x = colonne, y = ligne

bool tourBlanc = true;   // true = les blancs jouent, false = les noirs
int selectionX = -1;     // -1 = aucune pièce sélectionnée, sinon case (selectionX, selectionY)
int selectionY = -1;

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
  strip.begin();
  strip.show();
  strip.setBrightness(100);

  initPositionEnPassant();  // Position de test : pion blanc e2, pion noir d4 (c'est au blanc de jouer)
  Serial.println("Pret. Envoyer EP pour reinit, ? pour plateau, x,y pour selection, x1,y1 x2,y2 pour jouer.");
}

//==============================================================
//  Boucle principale (loop) — test en passant
//==============================================================
//
// À chaque tour : on affiche le plateau en LEDs, puis si une pièce est "sélectionnée"
// on affiche ses coups possibles en ROUGE par-dessus. Ensuite on lit une commande série si elle arrive.
//
// Commandes (moniteur série 115200, envoyer avec Entrée) :
//   EP         → réinitialise la position (pion blanc e2, pion noir d4). C'est au blanc de jouer.
//   ?          → affiche le plateau en texte dans le moniteur.
//   x,y        → sélectionne la pièce en (x,y). Seule la pièce du camp au trait peut être sélectionnée.
//                Les cases où elle peut aller s'allument en ROUGE (et restent allumées jusqu'au prochain coup).
//                Ex. "4,1" = pion blanc e2 ; "3,3" = pion noir d4.
//   x1,y1 x2,y2 → joue le coup de (x1,y1) vers (x2,y2). Ex. "4,1 4,3" = e2-e4 (double pas).
//

void loop() {
  afficherPlateauLED();
  if (selectionX >= 0 && selectionY >= 0 && plateau[selectionX][selectionY].getType() != AUCUN)
    calculerDeplacements(plateau[selectionX][selectionY]);
  strip.show();

  if (Serial.available() <= 0) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "EP") {
    initPositionEnPassant();
    tourBlanc = true;
    selectionX = -1;
    selectionY = -1;
    Serial.println("Position chargee. Aux blancs. Selectionnez 4,1 (pion blanc e2).");
    return;
  }

  if (cmd == "?") {
    afficherPlateauSerial();
    return;
  }

  // Commande "x,y" : sélectionner une pièce pour afficher ses coups en ROUGE.
  // On n'affiche les coups que si c'est bien le tour de cette pièce (blanc ou noir).
  if (cmd.length() == 3) {
int x = cmd.substring(0, 1).toInt();
    int y = cmd.substring(2, 3).toInt();
    if (plateau[x][y].getType() == AUCUN) {
      Serial.println("Case vide.");
    } else if ((plateau[x][y].getCouleur() == BLANC) != tourBlanc) {
      Serial.println("Pas a votre tour.");
    } else {
      selectionX = x;
      selectionY = y;
      Serial.println("Selection OK. Coups en ROUGE.");
    }
    return;
  }

  // Commande "x1,y1 x2,y2" : jouer le coup (origine -> destination).
  if (cmd.length() >= 7) {
    int x1 = cmd.substring(0, 1).toInt();
    int y1 = cmd.substring(2, 3).toInt();
    int x2 = cmd.substring(4, 5).toInt();
    int y2 = cmd.substring(6, 7).toInt();
    if (plateau[x1][y1].getType() == AUCUN) {
      Serial.println("Case origine vide.");
    } else if ((plateau[x1][y1].getCouleur() == BLANC) != tourBlanc) {
      Serial.println("Pas a votre tour.");
    } else {
      appliquerCoup(x1, y1, x2, y2);
      tourBlanc = !tourBlanc;
      selectionX = -1;
      selectionY = -1;
      Serial.println(tourBlanc ? "Aux blancs." : "Aux noirs.");
    }
    return;
  }

  Serial.println("Commande inconnue. EP / ? / x,y / x1,y1 x2,y2");
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
bool estCaseAttaquee(int cx, int cy, Couleur parQui);
void filtrerCoupsEnEchec(Piece &p, int X[], int Y[], int &nb);

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
  filtrerCoupsEnEchec(p, X, Y, nb);
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
  // Prise en passant : possible si une cible est définie sur la rangée devant nous (case vide)
  if (enPassantCol >= 0 && enPassantRow == y + dirY && (enPassantCol == x + 1 || enPassantCol == x - 1) && plateau[enPassantCol][enPassantRow].getType() == AUCUN) {
    X[nb] = enPassantCol; Y[nb] = enPassantRow; nb++;
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

// Roi : 8 cases autour + roque si possible
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
  // Roque : roi et tour jamais bougé, pas de pièce entre, roi et cases traversées non attaquées
  if (p.getNbDeplacements() == 0 && !estCaseAttaquee(x, y, (c == BLANC) ? NOIR : BLANC)) {
    int ligneRoi = (c == BLANC) ? 0 : 7;
    if (y != ligneRoi) { return nb; }
    // Petit roque (roi vers colonne 6, tour 7 -> 5)
    if (plateau[7][ligneRoi].getType() == TOUR && plateau[7][ligneRoi].getCouleur() == c && plateau[7][ligneRoi].getNbDeplacements() == 0) {
      bool voieLibre = true;
      for (int col = 5; col <= 6; col++) { if (plateau[col][ligneRoi].getType() != AUCUN) voieLibre = false; }
      if (voieLibre && !estCaseAttaquee(5, ligneRoi, (c == BLANC) ? NOIR : BLANC) && !estCaseAttaquee(6, ligneRoi, (c == BLANC) ? NOIR : BLANC)) {
        X[nb] = 6; Y[nb] = ligneRoi; nb++;
      }
    }
    // Grand roque (roi vers colonne 2, tour 0 -> 3)
    if (plateau[0][ligneRoi].getType() == TOUR && plateau[0][ligneRoi].getCouleur() == c && plateau[0][ligneRoi].getNbDeplacements() == 0) {
      bool voieLibre = true;
      for (int col = 1; col <= 3; col++) { if (plateau[col][ligneRoi].getType() != AUCUN) voieLibre = false; }
      if (voieLibre && !estCaseAttaquee(2, ligneRoi, (c == BLANC) ? NOIR : BLANC) && !estCaseAttaquee(3, ligneRoi, (c == BLANC) ? NOIR : BLANC)) {
        X[nb] = 2; Y[nb] = ligneRoi; nb++;
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

//------------------Prise en passant : API pour le binôme------------------

void setEnPassantTarget(int col, int row) {
  enPassantCol = col;
  enPassantRow = row;
}

void clearEnPassantTarget() {
  enPassantCol = -1;
  enPassantRow = -1;
}

//------------------Vérification échec / case attaquée------------------

// Retourne true si la case (cx, cy) est attaquée par au moins une pièce de la couleur parQui
bool estCaseAttaquee(int cx, int cy, Couleur parQui) {
  const int dirCavalier[8][2] = { { -2, 1 }, { -1, 2 }, { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 } };
  const int dirRoi[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      Piece &q = plateau[i][j];
      if (q.getType() == AUCUN || q.getCouleur() != parQui) continue;
      int px = q.getX(), py = q.getY();
      TypePiece tp = q.getType();
      if (tp == PION) {
        int dy = (parQui == BLANC) ? 1 : -1;
        if (cy == py + dy && (cx == px + 1 || cx == px - 1)) return true;
      } else if (tp == CAVALIER) {
        for (int d = 0; d < 8; d++) {
          if (px + dirCavalier[d][0] == cx && py + dirCavalier[d][1] == cy) return true;
        }
      } else if (tp == ROI) {
        for (int d = 0; d < 8; d++) {
          if (px + dirRoi[d][0] == cx && py + dirRoi[d][1] == cy) return true;
        }
      } else if (tp == FOU || tp == TOUR || tp == DAME) {
        int dx = (cx > px) ? 1 : (cx < px) ? -1 : 0;
        int dy = (cy > py) ? 1 : (cy < py) ? -1 : 0;
        if (dx == 0 && dy == 0) continue;
        if (tp == FOU && (dx == 0 || dy == 0)) continue;
        if (tp == TOUR && dx != 0 && dy != 0) continue;
        int steps = (abs(cx - px) > abs(cy - py)) ? abs(cx - px) : abs(cy - py);
        if (px + steps * dx != cx || py + steps * dy != cy) continue;  // (cx,cy) pas sur la ligne
        bool bloque = false;
        for (int s = 1; s < steps; s++) {
          int nx = px + s * dx, ny = py + s * dy;
          if (plateau[nx][ny].getType() != AUCUN) { bloque = true; break; }
        }
        if (!bloque) return true;
      }
    }
  }
  return false;
}

// Enlève de X[], Y[] les coups qui laisseraient notre roi en échec (modifie nb)
// Simulation sans appeler setPosition pour ne pas modifier nbDeplacements.
void filtrerCoupsEnEchec(Piece &p, int X[], int Y[], int &nb) {
  int fromX = p.getX(), fromY = p.getY();
  Couleur nous = p.getCouleur();
  Couleur adversaire = (nous == BLANC) ? NOIR : BLANC;
  for (int i = 0; i < nb; i++) {
    int toX = X[i], toY = Y[i];
    Piece sauveDest = plateau[toX][toY];
    plateau[toX][toY] = plateau[fromX][fromY];
    plateau[fromX][fromY].vider();
    int roiX, roiY;
    if (plateau[toX][toY].getType() == ROI) { roiX = toX; roiY = toY; }
    else {
      roiX = -1; roiY = -1;
      for (int a = 0; a < 8 && roiX < 0; a++)
        for (int b = 0; b < 8; b++)
          if (plateau[a][b].getType() == ROI && plateau[a][b].getCouleur() == nous) { roiX = a; roiY = b; break; }
    }
    bool enEchec = estCaseAttaquee(roiX, roiY, adversaire);
    plateau[fromX][fromY] = plateau[toX][toY];
    plateau[toX][toY] = sauveDest;
    if (enEchec) {
      X[i] = X[nb - 1]; Y[i] = Y[nb - 1];
      nb--;
      i--;
    }
  }
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

//------------------Affichage plateau en LEDs------------------

void afficherPlateauLED() {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint8_t idx = x + y * 8;
      if (plateau[x][y].getType() == NOIR) {
        setuLED(idx, strip.Color(255, 255, 0));
      } else if (plateau[x][y].getCouleur() == BLANC) {
        setuLED(idx, strip.Color(255, 255, 255));
      } else {
        setuLED(idx, strip.Color(0, 0, 0));
      }
    }
  }
}

//------------------Position de test prise en passant------------------

void initPositionEnPassant() {
  for (int x = 0; x < 8; x++)
    for (int y = 0; y < 8; y++)
      plateau[x][y].vider();
  plateau[4][1].reset(PION, BLANC, 4, 1);   // e2
  plateau[3][3].reset(PION, NOIR, 3, 3);     // d4
  clearEnPassantTarget();
}

//------------------Application d'un coup avec gestion en passant------------------

void appliquerCoup(int x1, int y1, int x2, int y2) {
  if (plateau[x1][y1].getType() == AUCUN) {
    Serial.println("Case origine vide.");
    return;
  }
  Piece &p = plateau[x1][y1];

  if (p.getType() == PION && (y2 - y1 == 2 || y2 - y1 == -2)) {
    setEnPassantTarget(x2, (y1 + y2) / 2);
  } else {
    clearEnPassantTarget();
  }

  bool priseEnPassant = (p.getType() == PION && x1 != x2 && plateau[x2][y2].getType() == AUCUN && enPassantCol == x2 && enPassantRow == y2);
  if (priseEnPassant) {
    int rowPionCapture = (p.getCouleur() == BLANC) ? y2 - 1 : y2 + 1;
    plateau[x2][rowPionCapture].vider();
  }

  plateau[x2][y2] = plateau[x1][y1];
  plateau[x2][y2].setPosition(x2, y2);
  plateau[x1][y1].vider();
  if (priseEnPassant) clearEnPassantTarget();

  Serial.println("Coup effectue.");
}

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
