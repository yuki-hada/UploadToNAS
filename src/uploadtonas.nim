import std/[os, times, strutils, strformat]

const LastRunFile = "last_run.txt"

proc errorStop() =
  stderr.writeLine "The program has stopped due to the above error. Press any key to exit."
  discard stdin.readLine()

proc getCurrentTimeString(): string =
  now().format("yyyy-MM-dd HH:mm:ss")

proc parseTimestamp(timestamp: string): Time =
  try:
    parse(timestamp, "yyyy-MM-dd HH:mm:ss").toTime()
  except TimeParseError:
    stderr.writeLine &"Failed to parse timestamp: {timestamp}"
    stderr.writeLine "Expected format: YYYY-MM-DD HH:MM:SS"
    errorStop()
    quit(1)

proc getTimestampFolder(): string =
  now().format("yyyy/yyyyMMddHHmm")

proc saveCurrentTime(sourcePath, savePath: string) =
  echo "Overwriting last_run.txt."
  let strDateTime = getCurrentTimeString()
  try:
    let f = open(LastRunFile, fmWrite)
    defer: f.close()
    f.writeLine strDateTime
    f.writeLine sourcePath
    f.writeLine savePath
  except IOError:
    stderr.writeLine "There were some problems opening the last_run.txt file."
    errorStop()

proc copyRecentFiles(srcDir, baseDestDir, timestamp: string) =
  let cutoffTime = parseTimestamp(timestamp)
  let timestampFolder = getTimestampFolder()
  let destDir = baseDestDir / timestampFolder

  var copied = false

  for relPath in walkDirRec(srcDir, relative=true):
    let srcPath = srcDir / relPath
    let lastWrite = getLastModificationTime(srcPath)
    if lastWrite > cutoffTime:
      if not copied:
        try:
          createDir(destDir)
        except OSError as e:
          stderr.writeLine &"Failed to create backup directory: {destDir}"
          stderr.writeLine &"Error: {e.msg}"
          errorStop()
          quit(1)

      let destPath = destDir / relPath

      try:
        createDir(destPath.parentDir())
      except OSError as e:
        stderr.writeLine &"Failed to create destination directory: {destPath.parentDir()}"
        stderr.writeLine &"Error: {e.msg}"
        errorStop()
        quit(1)

      try:
        copyFile(srcPath, destPath)
        copied = true
        echo &"Copied: {srcPath} -> {destPath}"
      except OSError as e:
        stderr.writeLine "Failed to copy file:"
        stderr.writeLine &"Source: {srcPath}"
        stderr.writeLine &"Destination: {destPath}"
        stderr.writeLine &"Error: {e.msg}"
        errorStop()
        quit(1)

  if copied:
    saveCurrentTime(srcDir, baseDestDir)
  else:
    echo "There is no new file."

proc initializeSettings() =
  echo "Performing initial setup."
  echo "Please enter the path of the folder to back up."
  let sourcePath = stdin.readLine()
  echo "Please enter the path where the backup will be saved."
  let savePath = stdin.readLine()
  saveCurrentTime(sourcePath, savePath)

proc main() =
  if fileExists(LastRunFile):
    echo "Loading last_run.txt."
    try:
      let lines = readFile(LastRunFile).splitLines()
      if lines.len < 3:
        stderr.writeLine "last_run.txt is malformed."
        errorStop()
        quit(1)

      let lastRunTime = lines[0]
      let sourcePath  = lines[1]
      let savePath    = lines[2]

      echo &"Previous execution time: {lastRunTime}"
      echo &"Source copy: {sourcePath}"
      echo &"Destination: {savePath}"

      if not (dirExists(sourcePath)):
        stderr.writeLine "Error: The source folder does not exist."
        errorStop()
        quit(1)
      if not (dirExists(savePath)):
        stderr.writeLine "Error: The destination folder does not exist. Please check the connection to the NAS."
        errorStop()
        quit(1)

      copyRecentFiles(sourcePath, savePath, lastRunTime)
    except IOError:
      stderr.writeLine "Failed to open the file."
  else:
    echo "last_run.txt does not exist."
    initializeSettings()

main()
