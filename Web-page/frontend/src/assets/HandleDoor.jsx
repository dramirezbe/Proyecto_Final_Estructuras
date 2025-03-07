import React from 'react';
import io from 'socket.io-client';
import './HandleDoor.css';


const socket = io("http://localhost:5000"); // Conéctate al servidor Express

const HandleDoor = ({ setDoorState}) => {
  // Función para el botón "Puerta"
  const handleCloseDoor = () => {
    setDoorState(0);
    socket.emit('dataWifi', 10);
  };

  const handleOpenDoor = () => {
    setDoorState(1);
    socket.emit('dataWifi', 11);
  };

  // Función para el botón "Puerta Temp."
  const handleTempDoor = () => {
    setDoorState(2);
    socket.emit('dataWifi', 12);
  };

  return (
    <div className="button-container">
      <button className="btn" onClick={handleOpenDoor}>
        Open
      </button>
      <button className="btn" onClick={handleCloseDoor}>
        Close
      </button>
      <button className="btn" onClick={handleTempDoor}>
        Temporal
      </button>
    </div>
  );
};

export default HandleDoor;
