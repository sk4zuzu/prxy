{ pkgs ? import <nixpkgs> {} }:

with pkgs;

stdenv.mkDerivation {
  name = "prxy-env";
  buildInputs = [
    cmake
    gnumake
    gcc
    glibc.static
  ];
}
