const path = require('path');
const express = require('express');

const app = express();
const isProduction = process.env.NODE_ENV === 'production';
const port = isProduction
    ? (Number.parseInt(process.env.PORT, 10) || 3000)
    : 3000;
const host = isProduction
    ? (process.env.HOST || '0.0.0.0')
    : '0.0.0.0';
const publicDir = __dirname;
const publicUrl = isProduction
    ? (process.env.PUBLIC_HOST || (host === '0.0.0.0' ? 'localhost' : host))
    : 'localhost';
const MOTOR_COUNT = 3;

function createMotorState(id) {
    return {
        id,
        action: 'idle',
        ms_par_tour: 2700,
        nbr: 1
    };
}

const motorStates = Array.from({ length: MOTOR_COUNT }, (_, index) => createMotorState(index + 1));

function getMotorState(motorId) {
    return motorStates.find((motor) => motor.id === motorId);
}

function parseMotorId(value) {
    const motorId = Number.parseInt(value, 10);
    return Number.isInteger(motorId) ? motorId : null;
}

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
        environment: isProduction ? 'production' : 'local',
        motors: motorStates
    });
});

app.get('/commande.json', (req, res) => {
    res.json({ motors: motorStates });
});

app.post('/lancer', (req, res) => {
    const motorId = parseMotorId(req.body.motorId);
    const requestedTurns = Number.parseInt(req.body.nbr, 10);
    const nbr = Number.isInteger(requestedTurns) ? requestedTurns : 1;
    const motor = getMotorState(motorId);

    if (!motor) {
        return res.status(400).json({ error: 'Moteur invalide' });
    }

    if (nbr < 1 || nbr > 8) {
        return res.status(400).json({ error: 'Le nombre de tours doit etre compris entre 1 et 8' });
    }

    if (motor.action !== 'idle') {
        return res.status(400).json({ error: `Le moteur ${motorId} est deja en cours` });
    }

    motor.action = 'run';
    motor.nbr = nbr;

    res.json({ success: true, motor: motorId });
});

app.post('/fini', (req, res) => {
    const motorId = parseMotorId(req.body.motorId);
    const motor = getMotorState(motorId);

    if (!motor) {
        return res.status(400).json({ error: 'Moteur invalide' });
    }

    motor.action = 'idle';
    console.log(`Rotation terminee pour le moteur ${motorId}.`);
    res.json({ status: 'ok', motor: motorId });
});

app.listen(port, host, () => {
    console.log(`Serveur lance sur http://${publicUrl}:${port}`);
    console.log(`Mode: ${isProduction ? 'production' : 'local'}`);
});
