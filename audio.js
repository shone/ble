'use strict';

const audioContext = new (window.AudioContext || window.webkitAudioContext);

const soundGain = audioContext.createGain();
soundGain.gain.value = .04;
soundGain.connect(audioContext.destination);

let tickSound = null;
fetch('tick.wav').then(async response => {
	const buffer = await response.arrayBuffer();
	tickSound = await audioContext.decodeAudioData(buffer);
});

let connectSound = null;
fetch('connect.wav').then(async response => {
	const buffer = await response.arrayBuffer();
	connectSound = await audioContext.decodeAudioData(buffer);
});

let errorSound = null;
fetch('error.wav').then(async response => {
	const buffer = await response.arrayBuffer();
	errorSound = await audioContext.decodeAudioData(buffer);
});

function playSound(sound) {
	if (!sound) {
		return;
	}
	const audioSource = audioContext.createBufferSource();
	audioSource.buffer = sound;
	audioSource.connect(soundGain);
	audioSource.start(0);
}
