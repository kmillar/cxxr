% Generated by roxygen2 (4.1.0): do not edit by hand
% Please edit documentation in R/capturer.R
\name{EligibleForCapture}
\alias{EligibleForCapture}
\title{Check if function is eligible for wrapping to capture arguments and return values

This function checks that supplied function for capture is not a keyword, operator or in the blacklist (functions like rm, .GlobalEnv, etc.)
This is an internal function and is supposed to be used in SetupCapture}
\usage{
EligibleForCapture(func)
}
\arguments{
\item{func}{function name to check}
}
\value{
TRUE/FALSE if can be captured or not
}
\description{
Check if function is eligible for wrapping to capture arguments and return values

This function checks that supplied function for capture is not a keyword, operator or in the blacklist (functions like rm, .GlobalEnv, etc.)
This is an internal function and is supposed to be used in SetupCapture
}
\seealso{
SetupCapture
}

