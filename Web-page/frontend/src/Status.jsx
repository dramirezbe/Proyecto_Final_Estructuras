import { useEffect, useState } from "react";
import io from "socket.io-client";

const socket = io("http://localhost:5000"); // Conéctate al servidor Express

const Status = () => {
  const [logs, setLogs] = useState([]);
  const [serverStatus, setServerStatus] = useState("Cargando...");

  useEffect(() => {
    // Escuchar logs desde el servidor
    socket.on("serverLog", (message) => {
      setLogs((prevLogs) => [...prevLogs, message]);
      setServerStatus("En funcionamiento");
    });

    // Manejo de desconexión
    socket.on("disconnect", () => {
      setServerStatus("Desconectado");
    });

    return () => {
      socket.off("serverLog");
      socket.off("disconnect");
    };
  }, []);

  return (
    <section className="estado-servidor">
      <h2>Estado del Servidor</h2>
      <div className="estado">
        <span
          className="indicador"
          style={{ backgroundColor: serverStatus === "En funcionamiento" ? "green" : "red" }}
        ></span>
        <span>{serverStatus}</span>
      </div>
      <div className="logs">
        <pre>
          {logs.map((log, index) => (
            <div key={index}>{log}</div>
          ))}
        </pre>
      </div>
    </section>
  );
};

export default Status;
