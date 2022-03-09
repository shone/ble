'use strict';

const bluetoothStatusEl = document.querySelector('#bluetooth-status');

(async function setup() {
	if (!navigator.bluetooth) {
		bluetoothStatusEl.textContent = 'Bluetooth API unavailable on this browser/OS';
		document.body.dataset.bluetoothState = 'unavailable';
		return;
	}

	while (true) {
		if (!await navigator.bluetooth.getAvailability()) {
			bluetoothStatusEl.textContent = 'Bluetooth unavailable';
			document.body.dataset.bluetoothState = 'unavailable';
			await new Promise(resolve => {
				navigator.bluetooth.onavailabilitychanged = event => {
					if (event.value) {
						resolve();
					}
				}
			});
		}

		document.body.dataset.bluetoothState = 'ready';
		bluetoothStatusEl.textContent = 'Ready';

		const result = await new Promise(resolve => {
			document.querySelector('#connect-button').onclick = event => {
				event.preventDefault();
				playSound(tickSound);
				resolve();
			}
			navigator.bluetooth.onavailabilitychanged = event => {
				if (!event.value) {
					resolve('bluetooth-unavailable');
				}
			}
		});
		if (result === 'bluetooth-unavailable') {
			playSound(errorSound);
			continue;
		}

		const error = await connectToDevice();
		if (error) {
			playSound(errorSound);
			alert(error);
			continue;
		}
	}
}());

async function connectToDevice() {

	document.body.dataset.bluetoothState = 'connecting';

	const serviceUuid = '2314f4ce-1847-49ec-879e-8fbfd895c877';

	bluetoothStatusEl.textContent = 'Getting Bluetooth device...';
	let bluetoothDevice = null;
	try {
		bluetoothDevice = await navigator.bluetooth.requestDevice({
			filters: [
				{services: [serviceUuid]},
			],
		});
	} catch(error) {
		return `Failed to get Bluetooth device: ${error}`;
	}

	bluetoothStatusEl.textContent = 'Connecting to GATT server...';
	let gattServer = null;
	try {
		gattServer = await bluetoothDevice.gatt.connect();
	} catch(error) {
		return `Failed to connect to Bluetooth GATT server: ${error}`;
	}

	bluetoothStatusEl.textContent = 'Getting primary service...';
	let service = null;
	try {
		service = await gattServer.getPrimaryService(serviceUuid);
	} catch(error) {
		return `Failed to get Bluetooth service: ${error}`;
	}

	bluetoothStatusEl.textContent = 'Getting characteristics...';
	let ledCharacteristic = null;
	let buttonsCharacteristic = null;
	try {
		const characteristics = await service.getCharacteristics();
		ledCharacteristic     = characteristics.find(c => c.uuid === '0ac1eae9-b54f-4b01-bd75-e820c22e1d5d');
		buttonsCharacteristic = characteristics.find(c => c.uuid === '40403bde-3593-473a-a286-c6c5813a692c');
		if (!ledCharacteristic)     return 'Failed to find buttons characteristic';
		if (!buttonsCharacteristic) return 'Failed to find LEDs characteristic';
	} catch(error) {
		return `Failed to get characteristics: ${error}`;
	}

	bluetoothStatusEl.textContent = 'Getting current LED state...';
	const ledState = await ledCharacteristic.readValue();

	bluetoothStatusEl.textContent = 'Connected to device';
	document.body.dataset.bluetoothState = 'connected';
	playSound(connectSound);

	document.getElementById('leds').onpointerdown = async event => {
		event.preventDefault();
		playSound(tickSound);
		const ledNumber = event.target.dataset.led;
		if (!ledNumber) {
			return;
		}
		const newLedState = ledState.getUint8(0) ^ (1 << ledNumber);
		ledState.setUint8(0, newLedState);
		try {
			await ledCharacteristic.writeValueWithResponse(ledState);
		} catch(error) {
			alert(`Error while writing to LED characteristic: ${error}`);
		}
	}

	buttonsCharacteristic.oncharacteristicvaluechanged = event => {
		const buttonNumber = event.target.value.getUint8(0);
		const buttonEl = document.querySelector(`#buttons .button-${buttonNumber}`);
		playSound(tickSound);
		buttonEl.animate([
			{ background: '#888' },
			{ background: 'white' },
		], {
			duration: 150,
		});
	}
	try {
		await buttonsCharacteristic.startNotifications();
	} catch(error) {
		return `Error while starting notifications on buttons characteristic: ${error}`;
	}

	return new Promise(resolve => {
		bluetoothDevice.ongattserverdisconnected = () => {
			resolve('Bluetooth GATT server connection lost');
		}
	});
}
