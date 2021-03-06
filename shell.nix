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

# vim:ts=2:sw=2:et:syn=nix:
