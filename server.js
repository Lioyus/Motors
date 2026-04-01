const path = require('path');
const express = require('express');

const app = express();
const isProduction = process.env.NODE_ENV === 'production';
const port = isProduction
    ? (Number.parseInt(process.env.PORT, 10) || 3000)
    : 3000;
const host = isProduction
    ? (process.env.HOST || '0.0.0.0')
    : '127.0.0.1';
const publicDir = __dirname;
const publicUrl = isProduction
    ? (process.env.PUBLIC_HOST || (host === '0.0.0.0' ? 'localhost' : host))
    : 'localhost';

app.set('trust proxy', true);

app.use(express.json());
app.use(express.static(publicDir));

app.use((req, res, next) => {
    res.setHeader('Cache-Control', 'no-store');
    next();
});

app.get('/', (req, res) => {
    res.sendFile(path.join(publicDir, 'index.html'));
});

app.get('/healthz', (req, res) => {
    res.status(200).json({
        status: 'ok',
        environment: isProduction ? 'production' : 'local'
    });
});

// État initial
let motorStatus = {
    action: "idle",
    ms_par_tour: 2000,
    nbr: 1
};

// L'ESP32 appelle cette route pour savoir quoi faire
app.get('/commande.json', (req, res) => {
    res.json(motorStatus);
});

// L'interface Web appelle cette route quand on clique sur le bouton
app.post('/lancer', (req, res) => {
    const requestedTurns = Number.parseInt(req.body.nbr, 10);
    const nbr = Number.isInteger(requestedTurns) ? requestedTurns : 1;

    if (nbr < 1 || nbr > 8) {
        return res.status(400).json({ error: "Le nombre de tours doit etre compris entre 1 et 8" });
    }

    if (motorStatus.action === "idle") {
        motorStatus.action = "run";
        motorStatus.nbr = nbr;
        res.json({ success: true });
    } else {
        res.status(400).json({ error: "Moteur déjà en cours" });
    }
});

// L'ESP32 appelle cette route quand il a fini le travail
app.post('/fini', (req, res) => {
    motorStatus.action = "idle";
    console.log("Rotation terminée, interface déverrouillée.");
    res.json({ status: "ok" });
});

app.listen(port, host, () => {
    console.log(`Serveur lancé sur http://${publicUrl}:${port}`);
    console.log(`Mode: ${isProduction ? 'production' : 'local'}`);
});
