// VVER-440

SetFactory("OpenCASCADE");

// --- Meshbeállítások ---
Mesh.MshFileVersion = 2.0;
Mesh.Algorithm = 6; // Frontal-Delaunay típusú mesh --> szép kb egyenlő oldalú háromszögek --> nem tudom, hogy ez jó választás-e
Mesh.CharacteristicLengthMin = 0.3;
Mesh.CharacteristicLengthMax = 20;

// --- Paraméterek ---
sqrt3 = Sqrt(3);
baseAngle = 0;        // hexagon forgatás --> (már 0 fok..)

rodPitch = 12.75;     // két szomsz pálca távolsága (középpont!)
rodRings = 6;         // 6 gyűrű 0. kihagyva --> 6+12+18+24+30+36=126 pálca
fuelRadius = 3.6;     // üa pellet sugara --> UO2
cladOuterRadius = 4.75; // cladding sugara --> vastagság = fuelRadius - ez = 1,15mm

moderatorGap = 1.0;   // külső pálca és a belső hatszög lapja közötti vízrés
wallThickness = 8;    // kazetta külső falának vastagsága --> acél

apothemInner = rodPitch * rodRings * sqrt3 / 2 + cladOuterRadius + moderatorGap;
innerFlat = 2 * apothemInner;
innerRadius = innerFlat / sqrt3;

apothemOuter = apothemInner + wallThickness;
outerFlat = 2 * apothemOuter;
outerRadius = outerFlat / sqrt3;

// --- Mesh méretek ---
karFuel = 0.9;
karClad = 0.5;
karModerator = 0.3;
karBoundary = 6.0;

// --- Hexagon (belső) ---
innerPts[] = {};
For k In {0:5}
  angle = baseAngle + k * Pi/3.;
  px = innerRadius * Cos(angle);
  py = innerRadius * Sin(angle);
  pt = newp;
  Point(pt) = {px, py, 0, karModerator};
  innerPts[] += {pt};
EndFor

innerLines[] = {};
For k In {0:4}
  ln = newl;
  Line(ln) = {innerPts[k], innerPts[k+1]};
  innerLines[] += {ln};
EndFor
ln = newl;
Line(ln) = {innerPts[5], innerPts[0]};
innerLines[] += {ln};

innerLoop = newll;
Curve Loop(innerLoop) = {innerLines[0], innerLines[1], innerLines[2], innerLines[3], innerLines[4], innerLines[5]};
innerSurface = news;
Plane Surface(innerSurface) = {innerLoop};

// --- Hexagon (külcső) ---
outerPts[] = {};
For k In {0:5}
  angle = baseAngle + k * Pi/3.;
  px = outerRadius * Cos(angle);
  py = outerRadius * Sin(angle);
  pt = newp;
  Point(pt) = {px, py, 0, karBoundary};
  outerPts[] += {pt};
EndFor

outerLines[] = {};
For k In {0:4}
  ln = newl;
  Line(ln) = {outerPts[k], outerPts[k+1]};
  outerLines[] += {ln};
EndFor
ln = newl;
Line(ln) = {outerPts[5], outerPts[0]};
outerLines[] += {ln};

outerLoop = newll;
Curve Loop(outerLoop) = {outerLines[0], outerLines[1], outerLines[2], outerLines[3], outerLines[4], outerLines[5]};
outerSurface = news;
Plane Surface(outerSurface) = {outerLoop};

boundaryDiff[] = BooleanDifference{ Surface{outerSurface}; Delete; }{ Surface{innerSurface}; };
boundarySurface = boundaryDiff[0];

// --- Fűtőelem rács ---
fuelSurfaces[] = {};
cladSurfaces[] = {};
rodMaterialSurfaces[] = {};

For q In {-rodRings:rodRings}
  For r In {-rodRings:rodRings}
    s = -q - r;
    If ((Abs(q) <= rodRings) && (Abs(r) <= rodRings)&& (Abs(s) <= rodRings))
      If (!((q == 0) && (r == 0) && (s == 0))) // Közepe skippelve!
        xc = rodPitch * (q + 0.5 * r);
        yc = rodPitch * (sqrt3 / 2.) * r;

        fuelSurf = news;
        Disk(fuelSurf) = {xc, yc, 0, fuelRadius};

        cladOuter = news;
        Disk(cladOuter) = {xc, yc, 0, cladOuterRadius};
        cladDiff[] = BooleanDifference{ Surface{cladOuter}; Delete; }{ Surface{fuelSurf}; };
        cladSurf = cladDiff[0];

        fuelSurfaces[] += {fuelSurf};
        cladSurfaces[] += {cladSurf};
        rodMaterialSurfaces[] += {fuelSurf};
        rodMaterialSurfaces[] += {cladSurf};
      EndIf
    EndIf
  EndFor
EndFor

// --- Moderátor ---
moderatorDiff[] = BooleanDifference{ Surface{innerSurface}; Delete; }{ Surface{rodMaterialSurfaces[]}; };
moderatorSurface = moderatorDiff[0]; // am is csak egy eleme van...

// --- Mesh méretek használata ---
MeshSize{ PointsOf{ Surface{fuelSurfaces[]}; } } = karFuel;
MeshSize{ PointsOf{ Surface{cladSurfaces[]}; } } = karClad;
MeshSize{ PointsOf{ Surface{moderatorSurface}; } } = karModerator;
MeshSize{ PointsOf{ Surface{boundarySurface}; } } = karBoundary;

// --- Physical groups.. ---
// 2D anyagterek
Physical Surface("Fuel", 1) = {fuelSurfaces[]};        // Fuel pellets
Physical Surface("Cladding", 2) = {cladSurfaces[]};    // Zircaloy cladding
Physical Surface("Moderator", 3) = {moderatorSurface}; // Moderátor
Physical Surface("Reflector", 4) = {boundarySurface};  // Hexagon wrapper / reflektor

// 1D határok
// - ÜA–köpeny határ: a pellet felületeinek határgörbéi
fuelCurves[] = Boundary{ Surface{fuelSurfaces[]}; };
Physical Curve("Fuel-Clad", 11) = {fuelCurves[]};

// - Köpeny–moderátor határ: a köpeny külső körei (cladSurfaces határából a fuel-curvék kivonva)
cladAllCurves[] = Boundary{ Surface{cladSurfaces[]}; };
cladModeratorCurves[] = cladAllCurves[];
cladModeratorCurves[] -= {fuelCurves[]};
Physical Curve("Clad-Moderator", 12) = {cladModeratorCurves[]};

// - Moderátor–reflektor határ: belső hexagon élei
Physical Curve("Moderator-Reflector", 13) = {innerLines[]};

// - Külső perem: külső hexagon élei
Physical Curve("Outer-Boundary", 14) = {outerLines[]};
