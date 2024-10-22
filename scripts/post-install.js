//@ts-check

const fs = require('fs');
const os = require('os');
const path = require('path');

const RELEASE_DIR = path.join(__dirname, '../build/Release');
const BUILD_FILES = [
  path.join(RELEASE_DIR, 'terminal-handoff.node'),
  path.join(RELEASE_DIR, 'terminal-handoff.pdb')
];

console.log('\x1b[32m> Cleaning release folder...\x1b[0m');

function cleanFolderRecursive(folder) {
  var files = [];
  if (fs.existsSync(folder)) {
    files = fs.readdirSync(folder);
    files.forEach(function(file,index) {
      var curPath = path.join(folder, file);
      if (fs.lstatSync(curPath).isDirectory()) { // recurse
        cleanFolderRecursive(curPath);
        fs.rmdirSync(curPath);
      } else if (BUILD_FILES.indexOf(curPath) < 0){ // delete file
        fs.unlinkSync(curPath);
      }
    });
  }
};

try {
  cleanFolderRecursive(RELEASE_DIR);
} catch(e) {
  console.log(e);
  process.exit(1);
}

process.exit(0);
