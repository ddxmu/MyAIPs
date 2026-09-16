#!/usr/bin/env swift
import AppKit
import CoreText
import Darwin
import Foundation

func cgColor(_ red: CGFloat, _ green: CGFloat, _ blue: CGFloat, _ alpha: CGFloat = 1) -> CGColor {
  CGColor(srgbRed: red, green: green, blue: blue, alpha: alpha)
}

func gradient(_ colors: [CGColor], _ locations: [CGFloat]) -> CGGradient {
  CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors as CFArray, locations: locations)!
}

func monogramPath() -> CGPath {
  let font = CTFontCreateWithName("AvenirNext-Heavy" as CFString, 430, nil)
  let attributes: [NSAttributedString.Key: Any] = [
    NSAttributedString.Key(kCTFontAttributeName as String): font
  ]
  let line = CTLineCreateWithAttributedString(NSAttributedString(string: "PS", attributes: attributes))
  var ascent: CGFloat = 0
  var descent: CGFloat = 0
  var leading: CGFloat = 0
  let textWidth = CGFloat(CTLineGetTypographicBounds(line, &ascent, &descent, &leading))
  let baseline = (1024 - ascent - descent) / 2 + descent
  let runs = CTLineGetGlyphRuns(line) as! [CTRun]
  let path = CGMutablePath()

  for run in runs {
    let count = CTRunGetGlyphCount(run)
    var glyphs = [CGGlyph](repeating: 0, count: count)
    var positions = [CGPoint](repeating: .zero, count: count)
    CTRunGetGlyphs(run, CFRange(location: 0, length: count), &glyphs)
    CTRunGetPositions(run, CFRange(location: 0, length: count), &positions)
    for index in 0..<count {
      guard let glyphPath = CTFontCreatePathForGlyph(font, glyphs[index], nil) else { continue }
      path.addPath(glyphPath, transform: CGAffineTransform(
        translationX: (1024 - textWidth) / 2 + positions[index].x,
        y: baseline + positions[index].y
      ))
    }
  }
  return path
}

func drawIcon(pixelSize: Int) -> Data {
  let colorSpace = CGColorSpaceCreateDeviceRGB()
  let context = CGContext(
    data: nil,
    width: pixelSize,
    height: pixelSize,
    bitsPerComponent: 8,
    bytesPerRow: pixelSize * 4,
    space: colorSpace,
    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue
  )!
  context.setAllowsAntialiasing(true)
  context.setShouldAntialias(true)
  context.scaleBy(x: CGFloat(pixelSize) / 1024, y: CGFloat(pixelSize) / 1024)

  let tile = CGRect(x: 28, y: 28, width: 968, height: 968)
  let tilePath = CGPath(roundedRect: tile, cornerWidth: 210, cornerHeight: 210, transform: nil)
  context.setShadow(offset: CGSize(width: 0, height: -16), blur: 44, color: cgColor(0.035, 0.025, 0.13, 0.42))
  context.addPath(tilePath)
  context.setFillColor(cgColor(0.05, 0.06, 0.16))
  context.fillPath()
  context.setShadow(offset: .zero, blur: 0, color: nil)

  context.saveGState()
  context.addPath(tilePath)
  context.clip()
  let background = gradient(
    [cgColor(0.16, 0.43, 0.98), cgColor(0.40, 0.31, 0.96), cgColor(0.68, 0.28, 0.89)],
    [0, 0.52, 1]
  )
  context.drawLinearGradient(
    background,
    start: CGPoint(x: 150, y: 900), end: CGPoint(x: 900, y: 100),
    options: [.drawsBeforeStartLocation, .drawsAfterEndLocation]
  )
  let highlight = gradient([cgColor(0.55, 0.80, 1, 0.28), cgColor(0.55, 0.80, 1, 0)], [0, 1])
  context.drawRadialGradient(
    highlight,
    startCenter: CGPoint(x: 280, y: 780), startRadius: 0,
    endCenter: CGPoint(x: 280, y: 780), endRadius: 670,
    options: [.drawsAfterEndLocation]
  )
  context.restoreGState()

  context.addPath(tilePath)
  context.setStrokeColor(cgColor(1, 1, 1, 0.22))
  context.setLineWidth(5)
  context.strokePath()

  let letters = monogramPath()
  let shadow = CGMutablePath()
  shadow.addPath(letters, transform: CGAffineTransform(translationX: 0, y: -18))
  context.addPath(shadow)
  context.setFillColor(cgColor(0.10, 0.04, 0.28, 0.30))
  context.fillPath()

  context.saveGState()
  context.addPath(letters)
  context.clip()
  let ink = gradient([cgColor(1, 1, 1), cgColor(0.85, 0.88, 1)], [0, 1])
  context.drawLinearGradient(
    ink,
    start: CGPoint(x: 330, y: 680), end: CGPoint(x: 700, y: 350),
    options: [.drawsBeforeStartLocation, .drawsAfterEndLocation]
  )
  context.restoreGState()

  let representation = NSBitmapImageRep(cgImage: context.makeImage()!)
  return representation.representation(using: .png, properties: [:])!
}

let scriptDirectory = URL(fileURLWithPath: CommandLine.arguments[0]).standardizedFileURL.deletingLastPathComponent()
let iconset = FileManager.default.temporaryDirectory
  .appendingPathComponent("myaips-icon-\(UUID().uuidString).iconset", isDirectory: true)
let files: [(String, Int)] = [
  ("icon_16x16.png", 16), ("icon_16x16@2x.png", 32),
  ("icon_32x32.png", 32), ("icon_32x32@2x.png", 64),
  ("icon_128x128.png", 128), ("icon_128x128@2x.png", 256),
  ("icon_256x256.png", 256), ("icon_256x256@2x.png", 512),
  ("icon_512x512.png", 512), ("icon_512x512@2x.png", 1024)
]
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
defer { try? FileManager.default.removeItem(at: iconset) }

for (name, size) in files {
  try drawIcon(pixelSize: size).write(to: iconset.appendingPathComponent(name), options: .atomic)
}
try drawIcon(pixelSize: 1024).write(to: scriptDirectory.appendingPathComponent("myaips-icon.png"), options: .atomic)

let iconutil = Process()
iconutil.executableURL = URL(fileURLWithPath: "/usr/bin/iconutil")
iconutil.arguments = ["--convert", "icns", "--output", scriptDirectory.appendingPathComponent("myaips.icns").path, iconset.path]
try iconutil.run()
iconutil.waitUntilExit()
guard iconutil.terminationStatus == 0 else {
  fputs("iconutil failed with exit code \(iconutil.terminationStatus)\n", stderr)
  exit(iconutil.terminationStatus)
}
