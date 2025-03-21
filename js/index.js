const saveButton = document.getElementById("submit");
const csvInput = document.getElementById("csv");
const resultDiv = document.getElementById("result");

let binaryData;

// handle escape characters
function splitRow(header) {
  let columns = [];
  let column = "";
  let inQuotes = false;
  for (let i = 0; i < header.length; i++) {
    let char = header[i];
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

const DATA_TYPE_INT8 = 0x11;
const DATA_TYPE_UINT8 = 0x12;
const DATA_TYPE_INT16 = 0x13;
const DATA_TYPE_INT32 = 0x14;
const DATA_TYPE_FLOAT64 = 0x21;
const DATA_TYPE_STRING = 0x01;
const lengths = {
  [DATA_TYPE_INT8]: 1,
  [DATA_TYPE_UINT8]: 1,
  [DATA_TYPE_INT16]: 2,
  [DATA_TYPE_INT32]: 4,
  [DATA_TYPE_FLOAT64]: 8,
  [DATA_TYPE_STRING]: 99,
};

let dataFile = new Uint8Array();
let indexFile = new Uint8Array();

function detectTypes(columns, rows) {
  let types = {};

  // fill the types with all the candidates
  for (let column of columns) {
    types[column] = Object.keys(lengths).map(Number);
  }

  // detect types of columns by going through each row (remove candidates if they don't match)
  for (const row of rows) {
    for (const index in row) {
      const column = columns[index];
      const value = row[index];
      types[column] = types[column].filter((candidate) => {
        const isInt = value.match(/^-?\d+$/);
        const isNum = value.match(/^-?\d*\.?\d*$/);
        const intValue = parseInt(value, 10);
        const floatValue = parseFloat(value);
        switch (candidate) {
          case DATA_TYPE_STRING:
            return true;
          case DATA_TYPE_INT8:
            return isInt && intValue <= 127 && intValue >= -128;
          case DATA_TYPE_UINT8:
            return isInt && intValue <= 255 && intValue >= 0;
          case DATA_TYPE_INT16:
            return isInt && intValue <= 32767 && intValue >= -32768;
          case DATA_TYPE_INT32:
            return isInt && intValue <= 2147483647 && intValue >= -2147483648;
          case DATA_TYPE_FLOAT64:
            return (
              isNum &&
              floatValue <= 1.7976931348623157e308 &&
              floatValue >= -1.7976931348623157e308
            );
        }
      });
    }
  }

  // select the candidate needs the least space
  columns.forEach((column) => {
    let smallest = 0;
    types[column].forEach((type, index) => {
      if (lengths[type] < lengths[types[column][smallest]]) {
        smallest = index;
      }
    });
    types[column] = types[column][smallest];
  });

  return types;
}

function convertDataToBinary(columns, types, data) {
  const dataFile = [];
  dataFile.push(0x11); // version
  dataFile.push(columns.length); // number of columns
  for (let column of columns) {
    let type = types[column];
    dataFile.push(type); // type of column
    dataFile.push(...new TextEncoder().encode(column), 0);
  }
  for (values of data) {
    for (let i = 0; i < values.length; i++) {
      let value = values[i];
      let column = columns[i];
      if (!column) break;

      if (types[column] === DATA_TYPE_STRING) {
        // Encode string and add a null terminator
        const encoded = new TextEncoder().encode(value);
        dataFile.push(...encoded, 0);
      } else if (types[column] === DATA_TYPE_INT8) {
        let buffer = new ArrayBuffer(1);
        new DataView(buffer).setInt8(0, parseInt(value, 10));
        dataFile.push(...new Uint8Array(buffer));
      } else if (types[column] === DATA_TYPE_UINT8) {
        let buffer = new ArrayBuffer(1);
        new DataView(buffer).setUint8(0, parseInt(value, 10));
        dataFile.push(...new Uint8Array(buffer));
      } else if (types[column] === DATA_TYPE_INT16) {
        let buffer = new ArrayBuffer(2);
        new DataView(buffer).setInt16(0, parseInt(value, 10), true);
        dataFile.push(...new Uint8Array(buffer));
      } else if (types[column] === DATA_TYPE_INT32) {
        let buffer = new ArrayBuffer(4);
        new DataView(buffer).setInt32(0, parseInt(value, 10), true);
        dataFile.push(...new Uint8Array(buffer));
      } else if (types[column] === DATA_TYPE_FLOAT64) {
        let buffer = new ArrayBuffer(8);
        new DataView(buffer).setFloat64(0, parseFloat(value), true);
        dataFile.push(...new Uint8Array(buffer));
      }
    }
  }
  return dataFile;
}

function printBinary(data) {
  const rowSize = 24;
  let binaryString = "", readableString = "";
  for (let i = 0, count = 0; i < data.length; i++) {
    const byte = data[i];
    binaryString += byte.toString(16).padStart(2, "0") + " ";
    readableString += byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : '.';
    count++;
    if (count === rowSize || i === data.length - 1) {
      binaryString += " ".repeat((rowSize-count) * 3) + " | " + readableString + '\n';
      readableString = '';
      count = 0;
    }
  }
  resultDiv.innerHTML = binaryString;
}

function convert() {
  const csv = csvInput.value;
  const rows = csv.split("\n");
  const headers = rows[0].split(",");
  const data = [];
  const input = rows.slice(1).join("\n") + "\n";
  let values = [];
  let currentValue = "";
  let inQuotes = false;
  for (let pos = 0; pos < input.length; pos++) {
    const char = input[pos];
    const nextChar = input[pos + 1];
    if (char === '"') {
      if (inQuotes) {
        if (nextChar === '"') {
          currentValue += char;
          pos++;
          continue;
        } else {
          inQuotes = false;
        }
      } else {
        inQuotes = true;
      }
    } else if (char === "," && !inQuotes) {
      values.push(currentValue);
      currentValue = "";
    } else if (char === "\n" && !inQuotes) {
      values.push(currentValue);
      currentValue = "";
      data.push(values);
      values = [];
    } else {
      currentValue += char;
    }
  }
  const types = detectTypes(headers, data);

  return convertDataToBinary(headers, types, data);
}

function onInputChange() {
  binaryData = convert();
  printBinary(binaryData);
}

csvInput.addEventListener("input", onInputChange);
onInputChange();

async function saveFile(binaryData) {
  try {
    // Convert the simple array into a Uint8Array.
    const typedArray = new Uint8Array(binaryData);

    const options = {
      suggestedName: "data.mdb",
      types: [
        {
          description: "Mini-DB Files",
          accept: { "application/octet-stream": [".mdb"] },
        },
      ],
    };

    // Open the native save file dialog.
    const fileHandle = await window.showSaveFilePicker(options);

    // Create a writable stream.
    const writableStream = await fileHandle.createWritable();

    // Write the typed array by wrapping it in an object with a data property.
    await writableStream.write({ type: 'write', data: typedArray });

    // Close the file to finalize the save.
    await writableStream.close();
  } catch (error) {
    console.error("Error saving file:", error);
  }
}

saveButton.addEventListener("click", () => { saveFile(binaryData); });
