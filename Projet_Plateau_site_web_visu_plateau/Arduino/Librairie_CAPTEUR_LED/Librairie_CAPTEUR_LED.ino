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
  void setNbDeplacements(int n) { nbDeplacements = n; }

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
void afficherPlateauLED();       // Met à jour les LEDs selon l'état du plateau
void syncPlateauDepuisCapteurs(); // Lit les 64 capteurs et met à jour plateau (PION blanc/noir)
void initPositionEnPassant();    // Réinit tour + en passant (plateau = capteurs)
void appliquerCoup(int x1, int y1, int x2, int y2);  // Met à jour en passant + tour (plateau = capteurs)

// À appeler par le binôme quand il applique un coup :
// - Après un double pas du pion : setEnPassantTarget(col, row) avec la case où un pion adverse peut atterrir en prenant en passant.
// - Sinon (coup normal ou tour adverse) : clearEnPassantTarget().
// - Prise en passant : quand le coup joué est une prise en passant, en plus du déplacement, retirer le pion capturé (à (colDest, rowDest - dirY) pour le camp qui vient de jouer).
// - Roque : si le roi va en (6, ligneRoi) ou (2, ligneRoi), déplacer aussi la tour (7,ligneRoi)->(5,ligneRoi) ou (0,ligneRoi)->(3,ligneRoi).
void setEnPassantTarget(int col, int row);
void clearEnPassantTarget();

Piece plateau[8][8];  // plateau[x][y], x = colonne, y = ligne

bool tourBlanc = true;
int fromX = -1, fromY = -1;  // Pièce en main : case d'origine (-1 = aucune)
int possibleMoveX[MAX_COUPS], possibleMoveY[MAX_COUPS], nbPossibleMoves = 0;
int captureRedCol = -1, captureRedRow = -1;  // Case à afficher en rouge (pièce mangée)
unsigned long captureRedUntil = 0;           // Jusqu'à quel moment (millis)
uint8_t prevType[64], prevColor[64];         // État précédent pour détecter soulèvement/pose
uint8_t prevNb[64];

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

  clearEnPassantTarget();
  tourBlanc = true;
  fromX = -1;
  fromY = -1;
  captureRedCol = -1;
  captureRedRow = -1;
  captureRedUntil = 0;
  for (int i = 0; i < 64; i++) prevType[i] = AUCUN;
  Serial.println("Pret. Placez les pions. Prenez une piece du camp au trait -> coups en BLEU. Posez sur une case bleue -> coup joue, case mangee en ROUGE.");
}

//==============================================================
//  Boucle principale (loop) — détection au plateau
//==============================================================
// Prendre une pièce du camp au trait -> ses coups possibles s'affichent en BLEU.
// Poser la pièce sur une case bleue -> coup joué ; si capture, la case de la pièce mangée devient ROUGE.
// Commande série : EP = réinit, ? = afficher plateau texte.

void loop() {
  syncPlateauDepuisCapteurs();

  Couleur campTrait = tourBlanc ? BLANC : NOIR;

  // Détection : pièce du camp au trait soulevée (était sur une case, maintenant vide)
  if (fromX < 0) {
    for (int j = 0; j < 8 && fromX < 0; j++) {
      for (int k = 0; k < 8; k++) {
        int idx = j + k * 8;
        if (prevType[idx] != AUCUN && (Couleur)prevColor[idx] == campTrait && plateau[j][k].getType() == AUCUN) {
          fromX = j;
          fromY = k;
          plateau[j][k].reset((TypePiece)prevType[idx], campTrait, j, k);
          plateau[j][k].setNbDeplacements(prevNb[idx]);
          nbPossibleMoves = genererCoupsPossibles(plateau[j][k], possibleMoveX, possibleMoveY);
          plateau[j][k].vider();
          break;
        }
      }
    }
  }

  // Détection : pièce posée sur une case des coups possibles
  if (fromX >= 0 && nbPossibleMoves > 0) {
    for (int i = 0; i < nbPossibleMoves; i++) {
      int tx = possibleMoveX[i], ty = possibleMoveY[i];
      if (plateau[tx][ty].getType() != AUCUN && plateau[tx][ty].getCouleur() == campTrait) {
        int capCol = -1, capRow = -1;
        if (prevType[tx + ty * 8] != AUCUN && (Couleur)prevColor[tx + ty * 8] != campTrait) {
          capCol = tx;
          capRow = ty;
        } else if (enPassantCol == tx && enPassantRow == ty) {
          capCol = tx;
          capRow = (campTrait == BLANC) ? ty - 1 : ty + 1;
        }
        if (capCol >= 0) {
          captureRedCol = capCol;
          captureRedRow = capRow;
          captureRedUntil = millis() + 2000;
        }
        appliquerCoup(fromX, fromY, tx, ty);
        tourBlanc = !tourBlanc;
        fromX = -1;
        fromY = -1;
        nbPossibleMoves = 0;
        break;
      }
    }
  }

  if (captureRedUntil > 0 && millis() > captureRedUntil) {
    captureRedCol = -1;
    captureRedRow = -1;
    captureRedUntil = 0;
  }

  afficherPlateauLED();
  if (fromX >= 0) {
    for (int i = 0; i < nbPossibleMoves; i++)
      setuLED(possibleMoveX[i] + possibleMoveY[i] * 8, strip.Color(0, 0, 255));
  }
  if (captureRedCol >= 0)
    setuLED(captureRedCol + captureRedRow * 8, strip.Color(255, 0, 0));
  strip.show();

  for (int j = 0; j < 8; j++) {
    for (int k = 0; k < 8; k++) {
      int idx = j + k * 8;
      prevType[idx] = plateau[j][k].getType();
      prevColor[idx] = plateau[j][k].getCouleur();
      prevNb[idx] = plateau[j][k].getNbDeplacements();
    }
  }

  if (Serial.available() <= 0) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;
  if (cmd == "EP") {
    initPositionEnPassant();
    fromX = -1;
    fromY = -1;
    nbPossibleMoves = 0;
    captureRedCol = -1;
    captureRedRow = -1;
    captureRedUntil = 0;
    return;
  }
  if (cmd == "?") afficherPlateauSerial();
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
    setuLED(X[i] + Y[i] * 8, strip.Color(0, 0, 255));  // LED bleue = case atteignable
  }
  strip.show();
}

//==============================================================
//  Affichage et débogage
//==============================================================

//------------------Synchronisation plateau depuis le hardware------------------

void syncPlateauDepuisCapteurs() {
  for (int k = 0; k < 8; k++) {
    for (int j = 0; j < 8; j++) {
      uint8_t idx = j + k * 8;
      if (presence_pion_blanc(idx)) {
        plateau[j][k].reset(PION, BLANC, j, k);
        plateau[j][k].setNbDeplacements((k == 1) ? 0 : 1);  // 0 si rangée de départ
      } else if (presence_pion_noir(idx)) {
        plateau[j][k].reset(PION, NOIR, j, k);
        plateau[j][k].setNbDeplacements((k == 6) ? 0 : 1);
      } else {
        plateau[j][k].vider();
      }
    }
  }
}

//------------------Affichage plateau en LEDs------------------

void afficherPlateauLED() {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint8_t idx = x + y * 8;
      if (plateau[x][y].getType() == AUCUN) {
        setuLED(idx, strip.Color(0, 0, 0));
      } else if (plateau[x][y].getCouleur() == BLANC) {
        setuLED(idx, strip.Color(255, 255, 255));
      } else {
        setuLED(idx, strip.Color(255, 255, 0));
      }
    }
  }
}

//------------------Position de test prise en passant------------------

void initPositionEnPassant() {
  clearEnPassantTarget();
  tourBlanc = true;
  fromX = -1;
  fromY = -1;
  nbPossibleMoves = 0;
  captureRedCol = -1;
  captureRedRow = -1;
  captureRedUntil = 0;
  Serial.println("Reinit. Placez pion blanc e2 (4,1), pion noir d4 (3,3).");
}

//------------------Application d'un coup avec gestion en passant------------------

// Met à jour uniquement la logique (en passant, tour). Les pièces = capteurs, pas de déplacement ici.
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

  if (p.getType() == PION && x1 != x2 && enPassantCol == x2 && enPassantRow == y2) {
    clearEnPassantTarget();  // prise en passant (le pion capturé est retiré par toi sur le plateau)
  }

  Serial.println("Coup enregistre. Deplacez la piece sur le plateau.");
}

//------------------Affichage plateau en texte (Serial)------------------

void afficherPlateauSerial() {
  Serial.println("\n    0    1    2    3    4    5    6    7   (X)");
  Serial.println("  +----+----+----+----+----+----+----+----+");

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
    Serial.println("  +----+----+----+----+----+----+----+----+");
  }
}
