class MiniDB {
  static DATA_TYPE_INT8 = 0x11;
  static DATA_TYPE_UINT8 = 0x12;
  static DATA_TYPE_INT16 = 0x13;
  static DATA_TYPE_INT32 = 0x14;
  static DATA_TYPE_FLOAT64 = 0x21;
  static DATA_TYPE_STRING = 0x01;

  static LENGTHS = {
    [MiniDB.DATA_TYPE_INT8]: 1,
    [MiniDB.DATA_TYPE_UINT8]: 1,
    [MiniDB.DATA_TYPE_INT16]: 2,
    [MiniDB.DATA_TYPE_INT32]: 4,
    [MiniDB.DATA_TYPE_FLOAT64]: 8,
    [MiniDB.DATA_TYPE_STRING]: 99,
  };

  static splitCSVRow(row) {
    const columns = [];
    let column = "";
    let inQuotes = false;

    for (let i = 0; i < row.length; i++) {
      const char = row[i];
      if (char === '"') {
        inQuotes = !inQuotes;
      } else if (char === "," && !inQuotes) {
        columns.push(column);
        column = "";
      } else {
        column += char;
      }
    }

    columns.push(column);
    return columns;
  }

  static parseCSVData(input) {
    const rows = input.split("\n");
    const headers = MiniDB.splitCSVRow(rows[0]);
    const data = [];

    const csvBody = rows.slice(1).join("\n") + "\n";
    let values = [];
    let currentValue = "";
    let inQuotes = false;

    for (let pos = 0; pos < csvBody.length; pos++) {
      const char = csvBody[pos];
      const nextChar = csvBody[pos + 1];

      if (char === '"') {
        if (inQuotes && nextChar === '"') {
          currentValue += '"';
          pos++;
        } else {
          inQuotes = !inQuotes;
        }
      } else if (char === "," && !inQuotes) {
        values.push(currentValue);
        currentValue = "";
      } else if (char === "\n" && !inQuotes) {
        values.push(currentValue);
        data.push(values);
        values = [];
        currentValue = "";
      } else {
        currentValue += char;
      }
    }

    return { headers, data };
  }

  static detectTypes(columns, rows) {
    const types = {};

    for (const column of columns) {
      types[column] = Object.keys(MiniDB.LENGTHS).map(Number);
    }

    for (const row of rows) {
      for (const i in row) {
        const column = columns[i];
        const value = row[i];
        types[column] = types[column].filter((type) => MiniDB.isTypeValid(type, value));
      }
    }

    for (const column of columns) {
      types[column] = types[column].reduce((smallest, type) => {
        return MiniDB.LENGTHS[type] < MiniDB.LENGTHS[smallest] ? type : smallest;
      }, types[column][0]);
    }

    return types;
  }

  static isTypeValid(type, value) {
    const isInt = /^-?\d+$/.test(value);
    const isNum = /^-?\d*\.?\d+$/.test(value);
    const intVal = parseInt(value, 10);
    const floatVal = parseFloat(value);

    switch (type) {
      case MiniDB.DATA_TYPE_STRING:
        return true;
      case MiniDB.DATA_TYPE_INT8:
        return isInt && intVal >= -128 && intVal <= 127;
      case MiniDB.DATA_TYPE_UINT8:
        return isInt && intVal >= 0 && intVal <= 255;
      case MiniDB.DATA_TYPE_INT16:
        return isInt && intVal >= -32768 && intVal <= 32767;
      case MiniDB.DATA_TYPE_INT32:
        return isInt && intVal >= -2147483648 && intVal <= 2147483647;
      case MiniDB.DATA_TYPE_FLOAT64:
        return isNum && Math.abs(floatVal) <= Number.MAX_VALUE;
      default:
        return false;
    }
  }

  static convertToBinary(columns, types, rows) {
    const dataFile = [0x11, columns.length];

    for (const column of columns) {
      const type = types[column];
      dataFile.push(type, ...new TextEncoder().encode(column), 0);
    }

    for (const row of rows) {
      for (let i = 0; i < columns.length; i++) {
        const column = columns[i];
        const value = row[i];

        if (types[column] === MiniDB.DATA_TYPE_STRING) {
          dataFile.push(...new TextEncoder().encode(value), 0);
        } else {
          const buffer = new ArrayBuffer(MiniDB.LENGTHS[types[column]]);
          const view = new DataView(buffer);
          const intVal = parseInt(value, 10);
          const floatVal = parseFloat(value);

          switch (types[column]) {
            case MiniDB.DATA_TYPE_INT8:
              view.setInt8(0, intVal); break;
            case MiniDB.DATA_TYPE_UINT8:
              view.setUint8(0, intVal); break;
            case MiniDB.DATA_TYPE_INT16:
              view.setInt16(0, intVal, true); break;
            case MiniDB.DATA_TYPE_INT32:
              view.setInt32(0, intVal, true); break;
            case MiniDB.DATA_TYPE_FLOAT64:
              view.setFloat64(0, floatVal, true); break;
          }

          dataFile.push(...new Uint8Array(buffer));
        }
      }
    }

    return dataFile;
  }

  static prettyPrintBinary(data, element) {
    const rowSize = 24;
    let hexLine = "", textLine = "", output = "";

    for (let i = 0, count = 0; i < data.length; i++, count++) {
      const byte = data[i];
      hexLine += byte.toString(16).padStart(2, "0") + " ";
      textLine += byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : ".";

      if (count === rowSize - 1 || i === data.length - 1) {
        hexLine += " ".repeat((rowSize - count - 1) * 3);
        output += `${hexLine} | ${textLine}\n`;
        hexLine = textLine = "";
        count = -1;
      }
    }

    element.innerText = output;
  }

  static async saveFile(binaryData) {
    try {
      const fileHandle = await window.showSaveFilePicker({
        suggestedName: "data.mdb",
        types: [{ description: "Mini-DB Files", accept: { "application/octet-stream": [".mdb"] } }],
      });

      const stream = await fileHandle.createWritable();
      await stream.write({ type: "write", data: new Uint8Array(binaryData) });
      await stream.close();
    } catch (err) {
      console.error("Save failed:", err);
    }
  }
}

// DOM handlers
const csvInput = document.getElementById("csv");
const resultDiv = document.getElementById("result");
const saveButton = document.getElementById("submit");

let binaryData = [];

function onInputChange() {
  const csv = csvInput.value;
  const { headers, data } = MiniDB.parseCSVData(csv);
  const types = MiniDB.detectTypes(headers, data);
  binaryData = MiniDB.convertToBinary(headers, types, data);
  MiniDB.prettyPrintBinary(binaryData, resultDiv);
}

csvInput.addEventListener("input", onInputChange);
saveButton.addEventListener("click", () => MiniDB.saveFile(binaryData));

// Initial render
onInputChange();
