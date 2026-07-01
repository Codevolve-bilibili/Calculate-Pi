import html
import os

TERMINAL_BG = "#1e1e1e"
TERMINAL_FG = "#d4d4d4"
PROMPT_FG = "#4ec9b0"
FONT_FAMILY = "Consolas, Monaco, 'Courier New', monospace"
FONT_SIZE = 14
LINE_HEIGHT = 20
PADDING = 16

def make_terminal_svg(title, lines, output_filename):
    """Generate a simple terminal-styled SVG from text lines."""
    max_len = max(len(line) for line in lines) if lines else 0
    width = max(480, max_len * FONT_SIZE * 0.6 + PADDING * 2)
    height = PADDING * 2 + len(lines) * LINE_HEIGHT + 28  # extra for title bar

    header_height = 28
    body_y = PADDING + header_height

    svg_parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-label="{html.escape(title)}" >',
        f'  <rect width="{width}" height="{height}" rx="8" ry="8" fill="{TERMINAL_BG}" />',
        f'  <rect width="{width}" height="{header_height}" rx="8" ry="8" fill="#2d2d2d" />',
        f'  <text x="{width / 2}" y="{header_height / 2 + 5}" fill="#cccccc" font-family="sans-serif" font-size="12" text-anchor="middle">{html.escape(title)}</text>',
    ]

    for i, line in enumerate(lines):
        y = body_y + i * LINE_HEIGHT
        # Colorize prompts like `$` or paths
        if line.startswith("$"):
            svg_parts.append(
                f'  <text x="{PADDING}" y="{y}" fill="{PROMPT_FG}" font-family="{FONT_FAMILY}" font-size="{FONT_SIZE}" xml:space="preserve">{html.escape(line)}</text>'
            )
        else:
            svg_parts.append(
                f'  <text x="{PADDING}" y="{y}" fill="{TERMINAL_FG}" font-family="{FONT_FAMILY}" font-size="{FONT_SIZE}" xml:space="preserve">{html.escape(line)}</text>'
            )

    svg_parts.append("</svg>")

    os.makedirs(os.path.dirname(output_filename), exist_ok=True)
    with open(output_filename, "w", encoding="utf-8") as f:
        f.write("\n".join(svg_parts))
    print(f"Created {output_filename}")


def main():
    assets_dir = "assets"

    help_lines = [
        "$ ./build/calculate_pi --help",
        "Calculate Pi - Chudnovsky binary splitting",
        "",
        "Usage:",
        "  CalculatePi [options]",
        "  CalculatePi --terms <N> [-o <path>] [--stats]",
        "",
        "Options:",
        "  -n, --terms <N>   Number of Chudnovsky series terms",
        "  -o, --output <path>  Write the result to the specified file",
        "      --stats       Print additional statistics",
        "  -h, --help        Show this help message",
        "",
        "If no arguments are given, the program enters interactive mode.",
    ]
    make_terminal_svg("calculate_pi --help", help_lines, f"{assets_dir}/screenshot_help.svg")

    compute_lines = [
        "$ ./build/calculate_pi --terms 100 --stats",
        "3.14159265358979323846264338327950288419716939937510...",
        "Terms: 100",
        "Digits: 1417",
        "Time: 0 ms",
    ]
    make_terminal_svg("calculate_pi --terms 100 --stats", compute_lines, f"{assets_dir}/screenshot_compute.svg")


if __name__ == "__main__":
    main()
